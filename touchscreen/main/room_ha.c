/* Direct HA REST client. The bearer credential is never put in a UI response,
 * URL, log or retry queue. TLS is verified by room_transport. */
#include "room_ha.h"
#include "room_settings.h"
#include "room_audio.h"
#include "room_feedback.h"
#include "room_transport.h"
#include <math.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#define HA_COUNT 11
#define HA_TTL_US 30000000LL
#define HA_IDS 32
typedef struct {const char *key,*entity,*label,*kind,*domain;} ha_spec_t;
#include "room_ha_entities.h"
typedef struct {char state[128],unit[8];double value,position;unsigned actions;bool available,has_value,has_position;int64_t at;} ha_state_t;
typedef struct {char id[81];int key,action;bool accepted;} ha_request_t;
static ha_state_t ha_states[HA_COUNT];
static ha_request_t ha_requests[HA_IDS];
static unsigned ha_next;
static room_laundry_tracker_t laundry_tracker[4];
static SemaphoreHandle_t ha_mutex,ha_command_mutex,ha_io_mutex;
static room_http_session_t *ha_session;
static atomic_bool ha_command_waiting;
static char ha_error[120]="Waiting for Home Assistant";
static atomic_bool ha_auth_failed;
static const char *ha_actions[]={"on","off","open","stop","close","start"};
static const char *ha_string(cJSON *j,const char *key){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";}
static bool ha_numeric(cJSON *j,const char *key,double *value){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;*value=v->valuedouble;return true;}
static bool ha_fresh(ha_state_t s){return s.available&&s.at>0&&esp_timer_get_time()-s.at<=HA_TTL_US;}
static const char *ha_service(int key,int action)
{
    const char *kind=ha_specs[key].kind;
    if(!strcmp(kind,"light")||!strcmp(kind,"switch"))return action==0?"turn_on":action==1?"turn_off":NULL;
    if(!strcmp(kind,"cover"))return action==2?"open_cover":action==3?"stop_cover":action==4?"close_cover":NULL;
    if(!strcmp(kind,"vacuum"))return action==5?"start":action==3?"stop":NULL;
    return NULL;
}
static void ha_global_error(int code)
{
    xSemaphoreTake(ha_mutex,portMAX_DELAY);
    if(code==401||code==403||ha_auth_failed)
        for(int k=0;k<HA_COUNT;++k)ha_states[k].available=false;
    if(code==401||code==403||ha_auth_failed){ha_auth_failed=true;snprintf(ha_error,sizeof(ha_error),"HA token rejected; update token in device settings and restart");}
    else snprintf(ha_error,sizeof(ha_error),"HA request failed; reconnecting (recent state retained)");
    xSemaphoreGive(ha_mutex);
}
static esp_err_t ha_http(const char *path,const char *post,void *buffer,size_t capacity,int *code,int *length)
{
    *code=0;*length=0;
    /* Only the configured HTTPS origin receives this credential. */
    if(ha_auth_failed||!*room_settings_ha_token()||strncmp(room_settings_ha_url(),"https://",8))return ESP_ERR_INVALID_STATE;
    size_t len=strlen(room_settings_ha_token());
    if(len>2048||strpbrk(room_settings_ha_token(),"\r\n"))return ESP_ERR_INVALID_ARG;
    char *bearer=malloc(len+8);if(!bearer)return ESP_ERR_NO_MEM;
    snprintf(bearer,len+8,"Bearer %s",room_settings_ha_token());
    xSemaphoreTake(ha_io_mutex,portMAX_DELAY);
    esp_err_t err=ha_auth_failed?ESP_ERR_INVALID_STATE:room_http_keepalive(&ha_session,room_settings_ha_url(),"Authorization",bearer,path,post,buffer,capacity,post?10000:(strstr(path,"/api/camera_proxy/")==path?15000:7000),code,length);
    if(*code==401||*code==403)ha_auth_failed=true;
    xSemaphoreGive(ha_io_mutex);
    /* No requests are repeated automatically, including ambiguous service timeouts. */
    memset(bearer,0,len+8);free(bearer);
    if(err!=ESP_OK||*code>=300){
        const char *hint=err==ESP_ERR_INVALID_STATE?"clock/client not ready":
            err==ESP_ERR_INVALID_ARG?"invalid HA configuration":"transport or HTTP error";
        ESP_LOGW("room_ha","HA request failed: %s; err=0x%x HTTP=%d",hint,(unsigned)err,*code);
    }
    if(*code==401||*code==403)ha_global_error(*code);
    return err;
}
static ha_state_t ha_normalize(int key,cJSON *j)
{
    ha_state_t s={0};
    if(strcmp(ha_string(j,"entity_id"),ha_specs[key].entity))return s;
    const char *state=ha_string(j,"state");
    snprintf(s.state,sizeof(s.state),"%s",state);
    s.at=esp_timer_get_time();s.available=*state&&strcmp(state,"unknown")&&strcmp(state,"unavailable")&&strcmp(state,"none");
    cJSON *attrs=cJSON_GetObjectItemCaseSensitive(j,"attributes");double features=0;
    ha_numeric(attrs,"supported_features",&features);
    unsigned flags=features>=0&&features<=UINT32_MAX&&floor(features)==features?(unsigned)features:0;
    for(int action=0;action<6;++action)if(ha_service(key,action)) {
        unsigned needed=!strcmp(ha_specs[key].kind,"cover")?(action==2?1:action==4?2:8):!strcmp(ha_specs[key].kind,"vacuum")?(action==5?8192:8):0;
        if(!needed||(flags&needed))s.actions|=1U<<action;
    }
    if(!strcmp(ha_specs[key].kind,"temperature")) {
        char *end;double value=strtod(state,&end);
        s.has_value=*state&&!*end&&isfinite(value);s.value=value;s.available&=s.has_value;
        const char *unit=ha_string(attrs,"unit_of_measurement");
        if(!strcmp(unit,"°F")||!strcmp(unit,"°C")||!strcmp(unit,"F")||!strcmp(unit,"C"))snprintf(s.unit,sizeof(s.unit),"%s",unit);
    }
    s.has_position=!strcmp(ha_specs[key].kind,"cover")&&ha_numeric(attrs,"current_position",&s.position)&&s.position>=0&&s.position<=100;
    return s;
}
static void ha_laundry_lost(void)
{
    xSemaphoreTake(ha_mutex,portMAX_DELAY);
    for(unsigned k=0;k<4;++k)room_laundry_observe(&laundry_tracker[k],"",false,esp_timer_get_time());
    xSemaphoreGive(ha_mutex);
}
/* 0: no completion; 1: reset only after a gap; 2: fresh completion sound. */
static int ha_laundry_sample(int key,const char *state,bool available,int64_t now)
{
    xSemaphoreTake(ha_mutex,portMAX_DELAY);
    bool notify=room_laundry_observe(&laundry_tracker[key-3],state,available,now);
    int result=notify?2:laundry_tracker[key-3].completed?1:0;
    xSemaphoreGive(ha_mutex);return result;
}
static bool ha_read(int key,char *buffer,size_t capacity)
{
    char path[160];int code,n;snprintf(path,sizeof(path),"/api/states/%s",ha_specs[key].entity);
    esp_err_t err=ha_http(path,NULL,buffer,capacity,&code,&n);
    if(code==404){if(key>=3&&key<=6)ha_laundry_sample(key,"",false,esp_timer_get_time());xSemaphoreTake(ha_mutex,portMAX_DELAY);memset(&ha_states[key],0,sizeof(ha_states[key]));xSemaphoreGive(ha_mutex);return true;}
    if(err!=ESP_OK||code!=200){ha_laundry_lost();ha_global_error(code);return false;}
    cJSON *j=cJSON_ParseWithLengthOpts(buffer,n+1,NULL,true);
    bool valid=cJSON_IsObject(j)&&!strcmp(ha_string(j,"entity_id"),ha_specs[key].entity)&&cJSON_IsString(cJSON_GetObjectItemCaseSensitive(j,"state"));
    if(valid){ha_state_t s=ha_normalize(key,j);xSemaphoreTake(ha_mutex,portMAX_DELAY);ha_states[key]=s;xSemaphoreGive(ha_mutex);
        if(key>=3&&key<=6){
            int completion=ha_laundry_sample(key,s.state,s.available,s.at);
            if(completion==2)room_audio_laundry_done(key);
            else if(completion==1)room_audio_set_notify(key,false);
            if(completion)ESP_LOGI("room_ha","%s cycle ended: Notify reset to Off%s",ha_specs[key].label,completion==1?" (no stale sound)":"");
        }
    }
    cJSON_Delete(j);if(!valid){ha_laundry_lost();ha_global_error(0);}return valid;
}
static void ha_poll_once(void)
{
    if(!*room_settings_ha_token()||ha_auth_failed)return;
    char *buffer=heap_caps_malloc(32768,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!buffer){ha_global_error(0);return;}
    bool ok=true;
    for(int k=0;k<HA_COUNT;++k){
        /* Release I/O between entities so a user command goes next. */
        if(ha_command_waiting){ok=false;break;}
        if(!ha_read(k,buffer,32768)){ok=false;break;}
    }
    free(buffer);
    if(ok){
        xSemaphoreTake(ha_mutex,portMAX_DELAY);bool recovered=ha_error[0]!=0;ha_error[0]=0;xSemaphoreGive(ha_mutex);
        if(recovered)ESP_LOGI("room_ha","Home Assistant connected; entity poll complete");
    }
}
static void ha_task(void *arg){for(;;){ha_poll_once();vTaskDelay(pdMS_TO_TICKS(5000));}}
void room_ha_start(void)
{
    ha_mutex=xSemaphoreCreateMutex();ha_command_mutex=xSemaphoreCreateMutex();ha_io_mutex=xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(ha_mutex&&ha_command_mutex&&ha_io_mutex?ESP_OK:ESP_ERR_NO_MEM);
    if(!*room_settings_ha_token())snprintf(ha_error,sizeof(ha_error),"Add your HA long-lived token in device settings");
    if(*room_settings_ha_token())ESP_LOGI("room_ha","Direct HA client started; waiting for Wi-Fi and valid system time");
    else ESP_LOGW("room_ha","HA disabled: add long-lived token in /settings");
    ESP_ERROR_CHECK(xTaskCreate(ha_task,"home_assistant",10240,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}
cJSON *room_ha_status(void)
{
    cJSON *root=cJSON_CreateObject(),*array=cJSON_AddArrayToObject(root,"items");bool connected=false;
    xSemaphoreTake(ha_mutex,portMAX_DELAY);
    for(int k=0;k<HA_COUNT;++k){ha_state_t s=ha_states[k];const ha_spec_t *spec=&ha_specs[k];bool available=ha_fresh(s);connected|=available;
        cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"key",spec->key);cJSON_AddStringToObject(item,"label",spec->label);cJSON_AddStringToObject(item,"kind",spec->kind);
        cJSON_AddStringToObject(item,"state",available?s.state:"unavailable");cJSON_AddBoolToObject(item,"available",available);cJSON_AddNumberToObject(item,"sample_us",(double)s.at);cJSON_AddStringToObject(item,"unit",s.unit);
        if(available&&s.has_value)cJSON_AddNumberToObject(item,"value",s.value);
        if(available&&s.has_position)cJSON_AddNumberToObject(item,"position",s.position);
        cJSON *a=cJSON_AddArrayToObject(item,"actions");if(available)for(int n=0;n<6;++n)if(s.actions&(1U<<n))cJSON_AddItemToArray(a,cJSON_CreateString(ha_actions[n]));
        cJSON_AddItemToArray(array,item);
    }
    cJSON_AddStringToObject(root,"error",ha_error);xSemaphoreGive(ha_mutex);
    cJSON_AddBoolToObject(root,"configured",*room_settings_ha_token()!=0);cJSON_AddBoolToObject(root,"connected",connected);cJSON_AddBoolToObject(root,"room_client",true);return root;
}
static cJSON *ha_result(const char *message){cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"error",message);return j;}
cJSON *room_ha_command(cJSON *p,int *code)
{
    *code=400;
    if(!cJSON_IsObject(p)||cJSON_GetArraySize(p)!=3)return ha_result("Expected key, action and request_id");
    const char *key=ha_string(p,"key"),*action=ha_string(p,"action"),*rid=ha_string(p,"request_id");int k=-1,a=-1;
    for(int n=0;n<HA_COUNT;++n)if(!strcmp(key,ha_specs[n].key))k=n;
    for(int n=0;n<6;++n)if(!strcmp(action,ha_actions[n]))a=n;
    if(k<0||a<0||strlen(rid)<8||strlen(rid)>80||!ha_service(k,a))return ha_result("Invalid or unsupported request");
    if(xSemaphoreTake(ha_command_mutex,0)!=pdTRUE){*code=409;return ha_result("Another command is in progress");}
    bool accepted=false;bool known=false;
    for(int n=0;n<HA_IDS;++n)if(!strcmp(rid,ha_requests[n].id)){
        if(ha_requests[n].key!=k||ha_requests[n].action!=a){xSemaphoreGive(ha_command_mutex);*code=409;return ha_result("request_id already used");}
        accepted=ha_requests[n].accepted;known=true;break;
    }
    if(!known){
        xSemaphoreTake(ha_mutex,portMAX_DELAY);ha_state_t state=ha_states[k];xSemaphoreGive(ha_mutex);
        if(!ha_fresh(state)||!(state.actions&(1U<<a))){xSemaphoreGive(ha_command_mutex);*code=409;return ha_result("Device unavailable or action unsupported");}
        unsigned slot=ha_next++%HA_IDS;ha_request_t *r=&ha_requests[slot];*r=(ha_request_t){.key=k,.action=a};snprintf(r->id,sizeof(r->id),"%s",rid);
        char path[128],payload[180];snprintf(path,sizeof(path),"/api/services/%s/%s",ha_specs[k].domain,ha_service(k,a));snprintf(payload,sizeof(payload),"{\"entity_id\":\"%s\"}",ha_specs[k].entity);
        ha_command_waiting=true;
        char *response=heap_caps_malloc(65536,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);int status=0,n;
        if(response){esp_err_t err=ha_http(path,payload,response,65536,&status,&n);accepted=err==ESP_OK&&status>=200&&status<300;r->accepted=accepted;
            if(!accepted)ha_global_error(status);
            else ha_read(k,response,65536);
            free(response);
        }
        ha_command_waiting=false;
    }
    xSemaphoreGive(ha_command_mutex);*code=200;cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"accepted",accepted);cJSON_AddStringToObject(j,"outcome",accepted?"service_completed":"unknown");cJSON_AddStringToObject(j,"request_id",rid);
    if(!accepted)cJSON_AddStringToObject(j,"message","Outcome unknown; check device state before retrying");
    return j;
}
esp_err_t room_ha_camera(void *buffer,size_t capacity,int *status,int *length)
{
    *status=503;*length=0;xSemaphoreTake(ha_mutex,portMAX_DELAY);bool available=ha_fresh(ha_states[10]);xSemaphoreGive(ha_mutex);
    if(!available||ha_command_waiting)return ESP_ERR_INVALID_STATE;
    char path[180];snprintf(path,sizeof(path),"/api/camera_proxy/%s?width=480&height=320",ha_specs[10].entity);
    return ha_http(path,NULL,buffer,capacity,status,length);
}

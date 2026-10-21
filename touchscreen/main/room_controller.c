#include "room_controller.h"
#include "room_transport.h"
#include "room_settings.h"
#include "wifi_setup.h"
#include "thermo_radio.h"
#include "sdkconfig.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t io;
static uint64_t client,sequence;
static tl_packet_t latest;
static int64_t received_at;
static atomic_bool fallback;
static atomic_int mqtt_state,sync_state;
/* 32-bit milliseconds avoid non-lock-free 64-bit atomics on Xtensa. */
static atomic_uint mqtt_at;
static void sync_task(void *arg);
static bool fresh(void){return received_at&&esp_timer_get_time()-received_at<4000000;}
void room_controller_start(void)
{
    io=xSemaphoreCreateMutex();ESP_ERROR_CHECK(io?ESP_OK:ESP_ERR_NO_MEM);
    client=((uint64_t)esp_random()<<32)|esp_random();if(!client)client=1;
    if(room_settings_radio_managed()){
        const cottage_integration_t *p=room_settings_integrations();tl_radio_start_config(NULL,p->radio_enabled,p->peer,p->key);
    }else tl_radio_start(NULL);
    ESP_ERROR_CHECK(xTaskCreate(sync_task,"controller_setup",6144,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}
bool room_controller_fallback(void){return fallback;}
bool room_controller_radio_ready(void){return tl_radio_configured();}
bool room_controller_radio_online(void){return tl_radio_online();}
static bool http(const tl_packet_t *p,tl_packet_t *out)
{
    if(!room_wifi_connected()||strlen(room_settings_controller_token())!=32)return false;
    char hex[sizeof(*p)*2+1],body[sizeof(*p)*2+20],response[512];int status=0,length=0;
    tl_hex(p,hex);snprintf(body,sizeof(body),"{\"packet\":\"%s\"}",hex);
    if(room_http(room_settings_controller_url(),"X-Thermo-Token",room_settings_controller_token(),
       "/api/link",body,response,sizeof(response),1800,&status,&length)!=ESP_OK||status!=200)return false;
    cJSON *j=cJSON_Parse(response),*v=cJSON_GetObjectItemCaseSensitive(j,"packet");
    bool ok=cJSON_IsString(v)&&tl_unhex(v->valuestring,out);cJSON_Delete(j);return ok;
}
static bool matching(const tl_packet_t *p,const tl_packet_t *r)
{return tl_valid(r)&&r->operation==TL_REPLY&&r->client==p->client&&r->sequence==p->sequence;}
/* Only a read-only handshake may try both transports. An ambiguous mutation is
 * never repeated. A later status read synchronizes authoritative state. */
static bool exchange(tl_packet_t p,tl_packet_t *r,bool probe)
{
    int64_t start=esp_timer_get_time();bool ok=false;
    if(probe){
        if(http(&p,r)&&matching(&p,r)){fallback=false;ok=true;}
        else if(tl_radio_exchange(&p,r)==ESP_OK&&matching(&p,r)){fallback=true;ok=true;}
    }else ok=fallback?(tl_radio_exchange(&p,r)==ESP_OK&&matching(&p,r)):(http(&p,r)&&matching(&p,r));
    if(ok&&esp_timer_get_time()-start<3000000){latest=*r;received_at=esp_timer_get_time();mqtt_state=(r->flags&TL_MQTT_ONLINE)?2:(r->flags&TL_MQTT_CONFIGURED)?1:0;mqtt_at=(uint32_t)(received_at/1000);}
    else if(probe){received_at=0;/* Keep HA disabled until a Wi-Fi controller status succeeds. */}
    return ok;
}
static cJSON *status_json(const tl_packet_t *r)
{
    cJSON *j=cJSON_CreateObject();
    if(r->flags&TL_HAS_TEMP)cJSON_AddNumberToObject(j,"temperature_f",r->temperature);
    if(r->flags&TL_HAS_TARGET)cJSON_AddNumberToObject(j,"target_f",r->target);
    cJSON_AddBoolToObject(j,"simulation",(r->flags&TL_SIMULATION)!=0);
    const char *modes[]={"hold","manual","direct","thermostat"};
    cJSON_AddStringToObject(j,"mode",r->mode<4?modes[r->mode]:"unknown");
    cJSON_AddStringToObject(j,"reason",r->reason);return j;
}
cJSON *room_controller_status(void)
{
    if(xSemaphoreTake(io,pdMS_TO_TICKS(100))!=pdTRUE)return NULL;
    tl_packet_t p=tl_request(TL_STATUS,client,++sequence),r;
    bool ok=exchange(p,&r,true);cJSON *j=ok&&r.result==TL_OK?status_json(&r):NULL;
    xSemaphoreGive(io);return j;
}
static bool mutation(uint8_t operation,float value,int *code)
{
    *code=503;if(xSemaphoreTake(io,pdMS_TO_TICKS(100))!=pdTRUE)return false;
    bool ok=false;int64_t sampled=esp_timer_get_time();
    if(!fresh()){tl_packet_t p=tl_request(TL_STATUS,client,++sequence),r;exchange(p,&r,true);}
    if(fresh()){
        tl_packet_t p=tl_request(operation,client,++sequence),r;
        p.controller_boot=latest.controller_boot;
        /* Lease is in controller monotonic time, with a conservative 1 s
         * transit budget. Older leases fail at the server; no wall clock. */
        p.deadline_ms=latest.deadline_ms+TL_LEASE_MS-1000;
        p.value=value;p.age_ms=operation==TL_SAMPLE?(uint32_t)((esp_timer_get_time()-sampled)/1000)+1000:0;
        bool delivered=exchange(p,&r,false);ok=delivered&&r.result==TL_OK;*code=ok?200:delivered?409:503;
    }
    xSemaphoreGive(io);return ok;
}
cJSON *room_controller_target(float target,int *code)
{
    if(!mutation(TL_TARGET,target,code))return NULL;
    cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",true);return j;
}
bool room_controller_sample(float temperature)
{int code;return mutation(TL_SAMPLE,temperature,&code);}

int room_controller_mqtt_state(void){
    int state=mqtt_state;uint32_t at=mqtt_at;
    if(state==2&&(!at||(uint32_t)((uint32_t)(esp_timer_get_time()/1000)-at)>10000))state=1;
    if(!state&&room_settings_integrations()->mqtt_enabled)state=1;
    return state;
}
const char *room_controller_sync_state(void){return sync_state==2?"Applied":sync_state==1?"Pending controller":"Not managed";}
static void sync_task(void *arg)
{
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(15000));cJSON *j=room_settings_controller_integrations();
        if(!j)continue;
        bool managed=cJSON_GetArraySize(j)>0;
        if(!managed){cJSON_Delete(j);continue;}sync_state=1;
        if(room_wifi_connected()&&!fallback&&strlen(room_settings_controller_token())==32){
            char *body=cJSON_PrintUnformatted(j);char response[256];int code=0,length;
            if(body){
                esp_err_t err=room_http(room_settings_controller_url(),"X-Thermo-Token",room_settings_controller_token(),
                    "/api/integrations",body,response,sizeof(response),3000,&code,&length);
                if(err==ESP_OK&&code==200){cJSON *reply=cJSON_Parse(response);if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(reply,"ok")))sync_state=2;cJSON_Delete(reply);}
                memset(body,0,strlen(body));free(body);
            }
        }
        cJSON_Delete(j);if(sync_state==2){vTaskDelete(NULL);return;}
        vTaskDelay(pdMS_TO_TICKS(45000));
    }
}

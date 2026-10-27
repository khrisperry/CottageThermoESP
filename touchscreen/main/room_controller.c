#include "room_controller.h"
#include "room_transport.h"
#include "room_discovery_probe.h"
#include "room_settings.h"
#include "wifi_setup.h"
#include "thermo_radio.h"
#include "sdkconfig.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_random.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif_ip_addr.h"
#include "mdns.h"
#include <strings.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t io;
static room_http_session_t *http_session;
static uint64_t client,sequence;
static tl_packet_t latest;
static int64_t received_at;
static atomic_bool fallback;
static atomic_int sync_state;
static char discovered_origin[64],discovered_address[16];
static uint32_t next_discovery_ms;
static unsigned manual_failures;
static bool discovery_verified;
static void sync_task(void *arg);
static bool fresh(void){return received_at&&esp_timer_get_time()-received_at<4000000;}

static bool controller_is_auto(void)
{
    const char *saved=room_settings_controller_url();
    return !*saved||!strcmp(saved,"auto");
}
static bool controller_can_use_mdns(void)
{
    /* Never silently downgrade an explicit HTTPS controller origin to HTTP. */
    return strncmp(room_settings_controller_url(),"https://",8)!=0;
}
static bool controller_is_local_name(void)
{
    const char *saved=room_settings_controller_url();
    return controller_can_use_mdns()&&strstr(saved,"CottageThermoController.local")!=NULL;
}
static const char *controller_origin(void)
{
    if(*discovered_origin&&controller_can_use_mdns()&&(controller_is_auto()||controller_is_local_name()||manual_failures>=2))
        return discovered_origin;
    return controller_is_auto()?"":room_settings_controller_url();
}
static void discover_controller(void)
{
    if(!room_wifi_connected())return;
    bool needed=controller_can_use_mdns()&&(controller_is_auto()||controller_is_local_name()||manual_failures>=2);
    if(!needed||*discovered_origin)return;
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if((int32_t)(now-next_discovery_ms)<0)return;
    next_discovery_ms=now+15000U;
    esp_ip4_addr_t ip={0};
    esp_err_t err=mdns_query_a("CottageThermoController",1200,&ip);
    if(err!=ESP_OK){
        ESP_LOGW("controller_discovery","mDNS lookup CottageThermoController.local failed: %s",esp_err_to_name(err));
        return;
    }
    char found_origin[64],found_ip[16];
    snprintf(found_ip,sizeof(found_ip),IPSTR,IP2STR(&ip));
    snprintf(found_origin,sizeof(found_origin),"http://%s",found_ip);
    const cottage_integration_t *pair=room_settings_integrations();
    room_discovery_result_t result={0};
    if(!room_discovery_probe_origin(found_origin,pair->radio_enabled,
                                   pair->peer,&result)){
        if(result.identity_mismatch)
            ESP_LOGW("controller_discovery",
                "Ignored discovered host %s: hostname or configured controller MAC differs",found_ip);
        else ESP_LOGW("controller_discovery",
            "mDNS returned %s but complete status failed: err=%d HTTP=%d bytes=%d; keeping ESP-NOW",
            found_ip,(int)result.http_error,result.http_status,result.response_bytes);
        return;
    }
    snprintf(discovered_origin,sizeof(discovered_origin),"%s",found_origin);
    snprintf(discovered_address,sizeof(discovered_address),"%s",found_ip);
    discovery_verified=true;
    ESP_LOGI("controller_discovery","Verified CottageThermoController.local at %s",found_ip);
}
cJSON *room_controller_discovery_status(void)
{
    cJSON *j=cJSON_CreateObject();if(!j)return NULL;
    if(!io||xSemaphoreTake(io,pdMS_TO_TICKS(50))!=pdTRUE){
        cJSON_AddStringToObject(j,"status","busy");return j;
    }
    cJSON_AddStringToObject(j,"hostname","CottageThermoController.local");
    cJSON_AddBoolToObject(j,"automatic",controller_is_auto());
    cJSON_AddBoolToObject(j,"verified",discovery_verified&&*discovered_origin);
    cJSON_AddStringToObject(j,"resolved_ip",discovered_address);
    cJSON_AddStringToObject(j,"active_url",controller_origin());
    cJSON_AddStringToObject(j,"source",*discovered_origin&&
        (controller_is_auto()||controller_is_local_name()||manual_failures>=2)?"mDNS":
        controller_is_auto()?"searching":"configured");
    xSemaphoreGive(io);return j;
}

void room_controller_start(void)
{
    io=xSemaphoreCreateMutex();ESP_ERROR_CHECK(io?ESP_OK:ESP_ERR_NO_MEM);
    client=((uint64_t)esp_random()<<32)|esp_random();if(!client)client=1;
    if(room_settings_radio_managed()){
        const cottage_integration_t *p=room_settings_integrations();tl_radio_start_config(NULL,p->radio_enabled,p->peer,p->key);
    }else tl_radio_start(NULL);
    ESP_ERROR_CHECK(xTaskCreate(sync_task,"controller_setup",6144,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}
bool room_controller_fallback(void){return fallback&&tl_radio_online();}
bool room_controller_radio_ready(void){return tl_radio_configured();}
bool room_controller_radio_online(void){return tl_radio_online();}
static bool http(const tl_packet_t *p,tl_packet_t *out)
{
    if(!room_wifi_connected())return false;
    const char *origin=controller_origin();
    if(!*origin)return false;
    char hex[sizeof(*p)*2+1],body[sizeof(*p)*2+20],response[512];int status=0,length=0;
    tl_hex(p,hex);snprintf(body,sizeof(body),"{\"packet\":\"%s\"}",hex);
    esp_err_t err=room_http_keepalive(&http_session,origin,NULL,NULL,
       "/api/link",body,response,sizeof(response),1800,&status,&length);
    if(err!=ESP_OK||status!=200){
        ESP_LOGW("controller_link","Controller %s/api/link: error=%d HTTP=%d",origin,(int)err,status);
        if(origin==discovered_origin){
            discovered_origin[0]=0;discovered_address[0]=0;discovery_verified=false;
            next_discovery_ms=0;
        }else if(manual_failures<100)++manual_failures;
        return false;
    }
    if(origin!=discovered_origin)manual_failures=0;
    cJSON *j=cJSON_Parse(response),*v=cJSON_GetObjectItemCaseSensitive(j,"packet");
    bool ok=cJSON_IsString(v)&&tl_unhex(v->valuestring,out);cJSON_Delete(j);return ok;
}
static bool matching(const tl_packet_t *p,const tl_packet_t *r)
{return tl_valid(r)&&r->operation==TL_REPLY&&r->client==p->client&&r->sequence==p->sequence;}
/* Only a read-only handshake may try both transports. An ambiguous mutation is
 * never repeated. A later status read synchronizes authoritative state. */
static bool exchange(tl_packet_t p,tl_packet_t *r,bool probe)
{
    int64_t start=esp_timer_get_time();bool ok=false,via_radio=fallback;
    if(probe){
        discover_controller();
        /* Discovery may block on multicast; the link lease starts at the
         * actual controller exchange, not the discovery lookup. */
        start=esp_timer_get_time();
        if(http(&p,r)&&matching(&p,r)&&r->result==TL_OK){via_radio=false;ok=true;}
        else {
            start=esp_timer_get_time();
            if(tl_radio_exchange(&p,r)==ESP_OK&&matching(&p,r)&&r->result==TL_OK){via_radio=true;ok=true;}
        }
    }else ok=fallback?(tl_radio_exchange(&p,r)==ESP_OK&&matching(&p,r)):(http(&p,r)&&matching(&p,r));
    if(ok&&esp_timer_get_time()-start<3000000){
        if(probe&&via_radio!=fallback){
            if(via_radio)ESP_LOGW("controller_link","Controller link switched to ESP-NOW");
            else ESP_LOGI("controller_link","Wi-Fi controller link restored");
        }
        if(probe)fallback=via_radio;
        latest=*r;received_at=esp_timer_get_time();
    }else {ok=false;if(probe)received_at=0;/* UI history never extends a command lease. */}
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
/* Read-only probes prove each transport independently, even with healthy Wi-Fi.
 * Do not change the selected transport, cached lease or authoritative state. */
cJSON *room_controller_test(void)
{
    if(xSemaphoreTake(io,pdMS_TO_TICKS(100))!=pdTRUE)return NULL;
    tl_packet_t p=tl_request(TL_STATUS,client,++sequence),r;
    discover_controller();
    bool wifi_ok=http(&p,&r)&&matching(&p,&r)&&r.result==TL_OK;
    p=tl_request(TL_STATUS,client,++sequence);
    bool radio_ok=tl_radio_exchange(&p,&r)==ESP_OK&&matching(&p,&r)&&r.result==TL_OK;
    cJSON *j=cJSON_CreateObject();
    cJSON_AddBoolToObject(j,"wifi_ok",wifi_ok);
    cJSON_AddBoolToObject(j,"radio_ready",tl_radio_configured());
    cJSON_AddBoolToObject(j,"radio_ok",radio_ok);
    char message[180];snprintf(message,sizeof(message),"Wi-Fi: %s. ESP-NOW: %s. Read-only test; no servo movement.",
        wifi_ok?"controller acknowledged":"no valid reply",
        radio_ok?"controller acknowledged":tl_radio_configured()?"no valid reply (check peer/key/channel)":"not configured");
    cJSON_AddStringToObject(j,"message",message);
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

const char *room_controller_sync_state(void){return sync_state==2?"Applied":sync_state==1?"Pending controller":"Not managed";}
static void sync_task(void *arg)
{
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(15000));cJSON *j=room_settings_controller_integrations();
        if(!j)continue;
        bool managed=cJSON_GetArraySize(j)>0;
        if(!managed){cJSON_Delete(j);continue;}sync_state=1;
        if(room_wifi_connected()&&!fallback&&xSemaphoreTake(io,pdMS_TO_TICKS(100))==pdTRUE){
            discover_controller();
            const char *origin=controller_origin();
            char *body=cJSON_PrintUnformatted(j);char response[256];int code=0,length;
            if(body&&*origin){
                esp_err_t err=room_http(origin,NULL,NULL,
                    "/api/integrations",body,response,sizeof(response),3000,&code,&length);
                if(err==ESP_OK&&code==200){cJSON *reply=cJSON_Parse(response);if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(reply,"ok")))sync_state=2;cJSON_Delete(reply);}
            }
            if(body){memset(body,0,strlen(body));free(body);}
            xSemaphoreGive(io);
        }
        cJSON_Delete(j);if(sync_state==2){vTaskDelete(NULL);return;}
        vTaskDelay(pdMS_TO_TICKS(45000));
    }
}

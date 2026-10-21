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
static bool fresh(void){return received_at&&esp_timer_get_time()-received_at<4000000;}
void room_controller_start(void)
{
    io=xSemaphoreCreateMutex();ESP_ERROR_CHECK(io?ESP_OK:ESP_ERR_NO_MEM);
    client=((uint64_t)esp_random()<<32)|esp_random();if(!client)client=1;
    tl_radio_start(NULL);
}
bool room_controller_fallback(void){return fallback;}
bool room_controller_radio_ready(void){return tl_radio_configured();}
bool room_controller_radio_online(void){return tl_radio_online();}
static bool http(const tl_packet_t *p,tl_packet_t *out)
{
    if(!room_wifi_connected()||strlen(CONFIG_ROOM_API_TOKEN)!=32)return false;
    char hex[sizeof(*p)*2+1],body[sizeof(*p)*2+20],response[512];int status=0,length=0;
    tl_hex(p,hex);snprintf(body,sizeof(body),"{\"packet\":\"%s\"}",hex);
    if(room_http(room_settings_controller_url(),"X-Thermo-Token",CONFIG_ROOM_API_TOKEN,
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
    if(ok&&esp_timer_get_time()-start<3000000){latest=*r;received_at=esp_timer_get_time();}
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

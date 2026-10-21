#include "room_ships.h"
#include "room_settings.h"
#include "room_controller.h"
#include "room_ota.h"
#include "wifi_setup.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>
#include "esp_crt_bundle.h"
#include "esp_app_desc.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
static SemaphoreHandle_t lock;
static room_ships_t model;
static atomic_bool active;
static void *pending_map;
static double pending_bounds[4];
/* Each worker exclusively owns its keep-alive client. Map loading cannot
 * delay snapshot heartbeats or device registration. */
static esp_http_client_handle_t http_client,map_http_client;
static char device_id[32];
#define SHIP_FRESH_US 35000000LL
typedef struct {uint8_t *out;size_t capacity,used;int64_t started,deadline,body_deadline;bool failed,map;} response_t;
static esp_err_t receive(esp_http_client_event_t *event)
{
    response_t *r=event->user_data;
    if(event->event_id!=HTTP_EVENT_ON_DATA)return ESP_OK;
    if(!r)return ESP_FAIL;
    int64_t now=esp_timer_get_time();
    /* TLS setup/server first-byte latency must not consume the body budget. */
    if(!r->body_deadline)r->body_deadline=now+(r->map?30000000LL:4000000LL);
    if(event->data_len<0||(size_t)event->data_len>r->capacity-r->used||now>r->deadline||now>r->body_deadline||room_ota_busy()){r->failed=true;return ESP_FAIL;}
    memcpy(r->out+r->used,event->data,event->data_len);r->used+=event->data_len;
    vTaskDelay(1);return ESP_OK;
}
static bool fetch(const char *path,void *out,size_t capacity,size_t *used,unsigned *elapsed,int *status,esp_err_t *transport)
{
    *used=0;*elapsed=0;*status=0;*transport=ESP_ERR_NO_MEM;
    char url[384];snprintf(url,sizeof(url),"%s%s",room_settings_ship_url(),path);
    bool map_request=strstr(path,"map.rgb565")!=NULL;
    int64_t started=esp_timer_get_time();
    response_t response={.out=out,.capacity=capacity,.started=started,.map=map_request,.deadline=started+(map_request?75000000LL:45000000LL)};
    int timeout=map_request?30000:10000;
    esp_http_client_handle_t *connection=map_request?&map_http_client:&http_client;
    if(!*connection){
        esp_http_client_config_t cfg={.url=url,.timeout_ms=timeout,.disable_auto_redirect=true,
            .keep_alive_enable=true,.max_authorization_retries=-1,
            .crt_bundle_attach=esp_crt_bundle_attach,.event_handler=receive};
        *connection=esp_http_client_init(&cfg);if(!*connection)return false;
    }
    esp_http_client_handle_t client=*connection;
    esp_err_t err=esp_http_client_set_url(client,url);
    if(err==ESP_OK)err=esp_http_client_set_timeout_ms(client,timeout);
    if(err==ESP_OK)err=esp_http_client_set_user_data(client,&response);
    if(err==ESP_OK)err=esp_http_client_set_header(client,"Accept",map_request?"application/octet-stream":"application/json");
    if(err==ESP_OK)err=esp_http_client_set_header(client,"X-Ship-Device-Id",device_id);
    if(err==ESP_OK)err=esp_http_client_set_header(client,"X-Ship-Firmware",esp_app_get_description()->version);
    char value[32];snprintf(value,sizeof(value),"%llu",(unsigned long long)(started/1000000));
    if(err==ESP_OK)err=esp_http_client_set_header(client,"X-Ship-Uptime",value);
    snprintf(value,sizeof(value),"%u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    if(err==ESP_OK)err=esp_http_client_set_header(client,"X-Ship-Heap",value);
    wifi_ap_record_t ap={0};int rssi=esp_wifi_sta_get_ap_info(&ap)==ESP_OK?ap.rssi:-127;
    if(rssi>0)rssi=0;
    if(rssi< -127)rssi=-127;
    snprintf(value,sizeof(value),"%d",rssi);
    if(err==ESP_OK)err=esp_http_client_set_header(client,"X-Ship-Rssi",value);
    if(err==ESP_OK)err=esp_http_client_perform(client);
    int code=esp_http_client_get_status_code(client);
    bool ok=err==ESP_OK&&!response.failed&&code==200&&esp_http_client_is_complete_data_received(client);
    *elapsed=(esp_timer_get_time()-started)/1000;*used=response.used;*status=code;*transport=err;
    esp_http_client_set_user_data(client,NULL);
    if(!ok){esp_http_client_cleanup(client);*connection=NULL;}
    return ok;
}
static bool healthy(bool feed)
{
    if(!lock)return false;
    xSemaphoreTake(lock,portMAX_DELAY);bool ok=model.service&&(!feed||model.feed)&&model.received_at&&esp_timer_get_time()-model.received_at<SHIP_FRESH_US;xSemaphoreGive(lock);
    return room_settings_ships_enabled()&&room_wifi_connected()&&ok;
}
bool room_ships_online(void){return healthy(true);}
bool room_ships_service_online(void){return healthy(false);}
void room_ships_active(bool value){active=value;}
void room_ships_snapshot(room_ships_t *out)
{
    if(!lock){memset(out,0,sizeof(*out));return;}
    xSemaphoreTake(lock,portMAX_DELAY);*out=model;xSemaphoreGive(lock);
    if(!room_wifi_connected()||!out->received_at||esp_timer_get_time()-out->received_at>=SHIP_FRESH_US)out->service=false;
}
void *room_ships_take_map(double bounds[4])
{
    if(!lock)return NULL;
    void *out;
    xSemaphoreTake(lock,portMAX_DELAY);out=pending_map;pending_map=NULL;
    if(out)memcpy(bounds,pending_bounds,sizeof(pending_bounds));
    xSemaphoreGive(lock);return out;
}
static void task(void *arg)
{
    int64_t next_poll=0;bool was_active=false;
    char *json=heap_caps_malloc(ROOM_SHIP_BODY_MAX+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!json){vTaskDelete(NULL);return;}
    for(;;){
        int64_t now=esp_timer_get_time();bool showing=active;
        if(showing&&!was_active)next_poll=0;
        was_active=showing;
        bool allowed=room_settings_ships_enabled()&&room_wifi_connected()&&!room_ota_busy();
        if(!strncmp(room_settings_ship_url(),"https://",8)&&time(NULL)<1767225600)allowed=false;
        if(allowed&&now>=next_poll){
            char path[96];snprintf(path,sizeof(path),"/api/v1/snapshot?range_miles=%u&limit=12",room_settings_ship_range());
            size_t used=0;unsigned elapsed=0;int status=0;esp_err_t transport=ESP_ERR_NO_MEM;room_ships_t *next=calloc(1,sizeof(*next));
            bool ok=next&&fetch(path,json,ROOM_SHIP_BODY_MAX,&used,&elapsed,&status,&transport);json[used]=0;
            if(ok)ok=room_ships_decode(json,used,next)&&next->range==room_settings_ship_range();
            xSemaphoreTake(lock,portMAX_DELAY);
            if(ok){next->received_at=esp_timer_get_time();next->bytes=used;next->latency_ms=elapsed;next->failures=model.failures;next->http_status=status;next->transport_error=transport;model=*next;}
            else{++model.failures;model.http_status=status;model.transport_error=transport;
                snprintf(model.error,sizeof(model.error),"Refresh failed: HTTP %d, err=0x%x, %u B / %u ms",status,(unsigned)transport,(unsigned)used,elapsed);}
            if(ok)ESP_LOGI("room_ships","%s: snapshot OK; %u ships, AIS %s, %u B / %u ms",device_id,next->count,next->feed?"online":"offline",(unsigned)used,elapsed);
            else ESP_LOGW("room_ships","%s: %s",device_id,model.error);
            xSemaphoreGive(lock);free(next);
            /* Service dashboard marks devices late after 15 s. Keep its existing
             * snapshot heartbeat at 10 s on every page; no extra request. */
            next_poll=esp_timer_get_time()+(ok?10000000LL:3000000LL);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
static void map_task(void *arg)
{
    int64_t next_map=0;bool map_loaded=false;
    for(;;){
        int64_t now=esp_timer_get_time();bool showing=active;
        bool allowed=room_settings_ships_enabled()&&room_wifi_connected()&&!room_ota_busy();
        if(!strncmp(room_settings_ship_url(),"https://",8)&&time(NULL)<1767225600)allowed=false;
        if(allowed&&showing&&!map_loaded&&now>=next_map){
            const size_t cap=40+456*400*2;uint8_t *map=heap_caps_malloc(cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            size_t used=0;unsigned elapsed=0;int status=0;esp_err_t transport=ESP_ERR_NO_MEM;char path[96];
            snprintf(path,sizeof(path),"/api/v1/map.rgb565?range_miles=%u&encoding=raw",room_settings_ship_range());
            bool ok=map&&fetch(path,map,cap,&used,&elapsed,&status,&transport)&&used==cap&&!memcmp(map,"TSM1",4)&&map[4]==200&&map[5]==1&&map[6]==144&&map[7]==1;
            double bounds[4]={0};if(ok){memcpy(bounds,map+8,sizeof(bounds));for(unsigned i=0;i<4;++i)if(!isfinite(bounds[i]))ok=false;
                ok=ok&&bounds[0]>=-180&&bounds[2]<=180&&bounds[1]>=-90&&bounds[3]<=90&&bounds[0]<bounds[2]&&bounds[1]<bounds[3];}
            if(ok&&!room_ota_busy()){
                memmove(map,map+40,456*400*2);xSemaphoreTake(lock,portMAX_DELAY);
                free(pending_map);pending_map=map;memcpy(pending_bounds,bounds,sizeof(bounds));
                xSemaphoreGive(lock);map=NULL;map_loaded=true;
            }
            free(map);next_map=esp_timer_get_time()+60000000;
            if(!ok)ESP_LOGW("room_ships","Map unavailable: HTTP %d err=0x%x, %u B / %u ms",status,(unsigned)transport,(unsigned)used,elapsed);
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
void room_ships_start(void)
{
    lock=xSemaphoreCreateMutex();ESP_ERROR_CHECK(lock?ESP_OK:ESP_ERR_NO_MEM);
    uint8_t mac[6];ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA,mac));
    snprintf(device_id,sizeof(device_id),"thermo-%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    /* ECC certificate verification can be CPU-bound. At idle priority on CPU0,
     * time slicing still runs IDLE0; UI runs separately on CPU1. Keep WDT enabled. */
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(task,"ship_tracker",8192,NULL,0,NULL,0)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreatePinnedToCore(map_task,"ship_map",8192,NULL,0,NULL,0)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}

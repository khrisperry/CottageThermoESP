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
typedef struct {uint8_t *out;size_t capacity,used;int64_t started,deadline;bool failed;} response_t;
static esp_err_t receive(esp_http_client_event_t *event)
{
    response_t *r=event->user_data;
    if(event->event_id!=HTTP_EVENT_ON_DATA)return ESP_OK;
    if(!r)return ESP_FAIL;
    if(event->data_len<0||(size_t)event->data_len>r->capacity-r->used||esp_timer_get_time()>r->deadline||room_ota_busy()){r->failed=true;return ESP_FAIL;}
    memcpy(r->out+r->used,event->data,event->data_len);r->used+=event->data_len;return ESP_OK;
}
static bool fetch(const char *path,void *out,size_t capacity,size_t *used,unsigned *elapsed)
{
    char url[384];snprintf(url,sizeof(url),"%s%s",room_settings_ship_url(),path);
    bool map_request=strstr(path,"map.rgb565")!=NULL;
    int64_t started=esp_timer_get_time();
    response_t response={.out=out,.capacity=capacity,.started=started,.deadline=started+(map_request?25000000LL:4000000LL)};
    esp_http_client_config_t cfg={.url=url,.timeout_ms=map_request?20000:3000,.disable_auto_redirect=true,
        .crt_bundle_attach=esp_crt_bundle_attach,.event_handler=receive,.user_data=&response};
    esp_http_client_handle_t client=esp_http_client_init(&cfg);if(!client)return false;
    esp_http_client_set_header(client,"Accept",strstr(path,"snapshot")?"application/json":"application/octet-stream");
    esp_err_t err=esp_http_client_perform(client);int code=esp_http_client_get_status_code(client);
    bool ok=err==ESP_OK&&!response.failed&&code==200&&esp_http_client_is_complete_data_received(client);
    *elapsed=(esp_timer_get_time()-response.started)/1000;*used=response.used;
    esp_http_client_cleanup(client);return ok;
}
bool room_ships_online(void)
{
    if(!lock)return false;
    xSemaphoreTake(lock,portMAX_DELAY);bool ok=model.service&&model.feed&&model.received_at&&esp_timer_get_time()-model.received_at<90000000;xSemaphoreGive(lock);
    return room_settings_ships_enabled()&&room_wifi_connected()&&!room_controller_fallback()&&ok;
}
void room_ships_active(bool value){active=value;}
void room_ships_snapshot(room_ships_t *out)
{
    if(!lock){memset(out,0,sizeof(*out));return;}
    xSemaphoreTake(lock,portMAX_DELAY);*out=model;xSemaphoreGive(lock);
    if(!room_wifi_connected()||room_controller_fallback()||!out->received_at||esp_timer_get_time()-out->received_at>90000000)out->service=false;
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
    int64_t next_poll=0,next_map=0;bool map_loaded=false,was_active=false;
    char *json=heap_caps_malloc(ROOM_SHIP_BODY_MAX+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!json){vTaskDelete(NULL);return;}
    for(;;){
        int64_t now=esp_timer_get_time();bool showing=active;
        if(showing&&!was_active)next_poll=0;
        was_active=showing;
        bool allowed=room_settings_ships_enabled()&&room_wifi_connected()&&!room_controller_fallback()&&!room_ota_busy();
        if(!strncmp(room_settings_ship_url(),"https://",8)&&time(NULL)<1767225600)allowed=false;
        if(allowed&&now>=next_poll){
            char path[96];snprintf(path,sizeof(path),"/api/v1/snapshot?range_miles=%u&limit=12",room_settings_ship_range());
            size_t used=0;unsigned elapsed=0;room_ships_t *next=calloc(1,sizeof(*next));
            bool ok=next&&fetch(path,json,ROOM_SHIP_BODY_MAX,&used,&elapsed);json[used]=0;
            if(ok)ok=room_ships_decode(json,used,next)&&next->range==room_settings_ship_range();
            xSemaphoreTake(lock,portMAX_DELAY);
            if(ok){next->received_at=esp_timer_get_time();next->bytes=used;next->latency_ms=elapsed;next->failures=model.failures;model=*next;}
            else{model.service=false;++model.failures;snprintf(model.error,sizeof(model.error),"Ship service unavailable; retrying");}
            xSemaphoreGive(lock);free(next);
            next_poll=esp_timer_get_time()+(showing?10000000LL:60000000LL);
        }
        if(allowed&&showing&&!map_loaded&&now>=next_map){
            const size_t cap=40+456*400*2;uint8_t *map=heap_caps_malloc(cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            size_t used=0;unsigned elapsed=0;char path[96];
            snprintf(path,sizeof(path),"/api/v1/map.rgb565?range_miles=%u&encoding=raw",room_settings_ship_range());
            bool ok=map&&fetch(path,map,cap,&used,&elapsed)&&used==cap&&!memcmp(map,"TSM1",4)&&map[4]==200&&map[5]==1&&map[6]==144&&map[7]==1;
            double bounds[4]={0};if(ok){memcpy(bounds,map+8,sizeof(bounds));for(unsigned i=0;i<4;++i)if(!isfinite(bounds[i]))ok=false;
                ok=ok&&bounds[0]>=-180&&bounds[2]<=180&&bounds[1]>=-90&&bounds[3]<=90&&bounds[0]<bounds[2]&&bounds[1]<bounds[3];}
            if(ok&&!room_ota_busy()){
                memmove(map,map+40,456*400*2);xSemaphoreTake(lock,portMAX_DELAY);
                free(pending_map);pending_map=map;memcpy(pending_bounds,bounds,sizeof(bounds));
                xSemaphoreGive(lock);map=NULL;map_loaded=true;
            }
            free(map);next_map=esp_timer_get_time()+60000000;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
void room_ships_start(void)
{
    lock=xSemaphoreCreateMutex();ESP_ERROR_CHECK(lock?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(task,"ship_tracker",8192,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}

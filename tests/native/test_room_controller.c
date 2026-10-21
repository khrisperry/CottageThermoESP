/* Actual panel transport selection with mocked HTTP and encrypted radio. */
#include <assert.h>
#include <stdio.h>
#include "../../touchscreen/main/room_controller.c"
static int64_t clock_us=1000000;
static bool wifi=true,http_up=true,radio_up=true,drop_reply=false;
static int http_calls,radio_calls,applications;
static tl_guard_t guard;
static uint64_t server_boot=123;
static float server_target=65;
int64_t esp_timer_get_time(void){return clock_us;}
uint32_t esp_random(void){return 42;}
void *xSemaphoreCreateMutex(void){return (void*)1;}
int xSemaphoreTake(void *s,int wait){return 1;}
int xSemaphoreGive(void *s){return 1;}
bool room_wifi_connected(void){return wifi;}
const char *room_settings_controller_url(void){return "http://controller.test";}
void tl_radio_start(tl_radio_handler_t handler){}
bool tl_radio_configured(void){return true;}
bool tl_radio_online(void){return radio_up;}
const char *room_settings_controller_token(void){return CONFIG_ROOM_API_TOKEN;}
bool room_settings_radio_managed(void){return false;}
const cottage_integration_t *room_settings_integrations(void){static cottage_integration_t p;return &p;}
cJSON *room_settings_controller_integrations(void){return cJSON_CreateObject();}
void tl_radio_start_config(tl_radio_handler_t cb,bool enabled,const char *peer,const char *key){}
int xTaskCreate(void(*f)(void*),const char*n,int a,void*b,int c,void*d){return 1;}
void vTaskDelay(int n){}
void vTaskDelete(void *p){}
static void server(const tl_packet_t *p,tl_packet_t *r)
{
    int result=tl_check(&guard,p,server_boot,clock_us/1000);
    if(result==TL_OK&&p->operation!=TL_STATUS){guard.sequence=p->sequence;++applications;if(p->operation==TL_TARGET)server_target=p->value;}
    *r=tl_request(TL_REPLY,p->client,p->sequence);r->result=result;r->controller_boot=server_boot;
    r->deadline_ms=clock_us/1000;r->flags=TL_HAS_TARGET|TL_SIMULATION;r->target=server_target;
}
esp_err_t tl_radio_exchange(const tl_packet_t *p,tl_packet_t *r)
{
    ++radio_calls;if(!radio_up)return ESP_FAIL;server(p,r);return drop_reply&&p->operation!=TL_STATUS?ESP_FAIL:ESP_OK;
}
esp_err_t room_http(const char *origin,const char *header,const char *token,const char *path,const char *post,void *out,size_t cap,int timeout,int *status,int *len)
{
    ++http_calls;if(!http_up)return ESP_FAIL;
    assert(!strcmp(path,"/api/link"));cJSON *j=cJSON_Parse(post),*v=cJSON_GetObjectItemCaseSensitive(j,"packet");tl_packet_t p,r;
    assert(tl_unhex(v->valuestring,&p));cJSON_Delete(j);server(&p,&r);
    if(drop_reply&&p.operation!=TL_STATUS)return ESP_FAIL;
    char hex[sizeof(r)*2+1];tl_hex(&r,hex);*len=snprintf(out,cap,"{\"packet\":\"%s\"}",hex);*status=200;return ESP_OK;
}
int main(void)
{
    room_controller_start();cJSON *j=room_controller_status();assert(j&&!room_controller_fallback());cJSON_Delete(j);
    int status;drop_reply=true;int before_radio=radio_calls;
    j=room_controller_target(67,&status);assert(!j&&status==503&&applications==1&&radio_calls==before_radio);
    /* Unknown HTTP outcome is reconciled by status, never retried over radio. */
    drop_reply=false;j=room_controller_status();assert(cJSON_GetObjectItem(j,"target_f")->valuedouble==67);cJSON_Delete(j);
    http_up=false;wifi=false;j=room_controller_status();assert(j&&room_controller_fallback());cJSON_Delete(j);
    j=room_controller_target(68,&status);assert(j&&applications==2);cJSON_Delete(j);
    /* Restart invalidates outstanding controller leases until next status. */
    ++server_boot;j=room_controller_target(69,&status);assert(!j&&applications==2);
    j=room_controller_status();assert(j);cJSON_Delete(j);j=room_controller_target(69,&status);assert(j&&applications==3);cJSON_Delete(j);
    radio_up=false;clock_us+=3600000000LL;j=room_controller_status();assert(!j);
    j=room_controller_target(70,&status);assert(!j&&applications==3);
    http_up=true;wifi=true;j=room_controller_status();assert(j&&!room_controller_fallback());cJSON_Delete(j);
    assert(room_controller_sample(66));assert(applications==4);
    mqtt_state=2;mqtt_at=clock_us/1000;assert(room_controller_mqtt_state()==2);clock_us+=11000000;assert(room_controller_mqtt_state()==1);
    puts("PASS: Wi-Fi/ESP-NOW selection, ambiguous no-retry, restart, hour outage and Wi-Fi recovery");
}

/* Profiles are immutable during a boot; saves atomically persist then restart.
 * Existing v0.10 credentials migrate without erasing NVS. */
#include "room_settings.h"
#include "room_secrets.h"
#include "wifi_setup.h"
#include "room_audio.h"
#include "room_display.h"
#include "room_controller.h"
#include "room_ota.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define SETTINGS_MAGIC 0x43544732U
#define OLD_MAGIC 0x43544731U
typedef struct {uint32_t magic;char ha[256],token[2049],controller[256];} old_settings_t;
typedef struct {
    uint32_t magic;char ha[256],token[2049],controller[256],controller_token[33];
    char ship_url[256];unsigned ship_range;
    bool ships_enabled,mqtt_managed,radio_managed,mqtt_password_known,mqtt_clear_password;
    cottage_integration_t integrations;
} settings_t;
static settings_t current;
static bool reboot_pending;
extern const unsigned char settings_start[] asm("_binary_settings_html_start");
extern const unsigned char settings_end[] asm("_binary_settings_html_end");
const char *room_settings_ha_url(void){return current.ha;}
const char *room_settings_ha_token(void){return current.token;}
const char *room_settings_controller_url(void){return current.controller;}
const char *room_settings_controller_token(void){return current.controller_token;}
const char *room_settings_ship_url(void){return current.ship_url;}
bool room_settings_ships_enabled(void){return current.ships_enabled;}
unsigned room_settings_ship_range(void){return current.ship_range;}
const cottage_integration_t *room_settings_integrations(void){return &current.integrations;}
bool room_settings_radio_managed(void){return current.radio_managed;}
static bool printable(const char *s,size_t max)
{
    if(strlen(s)>max)return false;
    for(const unsigned char *p=(const unsigned char*)s;*p;++p)if(*p<33||*p>126)return false;
    return true;
}
void room_settings_init(void)
{
    current.magic=SETTINGS_MAGIC;current.ships_enabled=true;current.ship_range=30;
    snprintf(current.ha,sizeof(current.ha),"%s",COTTAGE_HA_URL);
    snprintf(current.token,sizeof(current.token),"%s",COTTAGE_HA_TOKEN);
    snprintf(current.controller,sizeof(current.controller),"%s",CONFIG_ROOM_CONTROLLER_URL);
    snprintf(current.controller_token,sizeof(current.controller_token),"%s",CONFIG_ROOM_API_TOKEN);
    snprintf(current.ship_url,sizeof(current.ship_url),"https://ships.freshwater-tawas.us");
    snprintf(current.integrations.prefix,sizeof(current.integrations.prefix),"homeassistant");
    snprintf(current.integrations.name,sizeof(current.integrations.name),"Cottage thermostat");
#if CONFIG_COTTAGE_ESPNOW
    current.integrations.radio_enabled=true;
    snprintf(current.integrations.peer,sizeof(current.integrations.peer),"%s",CONFIG_COTTAGE_ESPNOW_PEER);
    snprintf(current.integrations.key,sizeof(current.integrations.key),"%s",CONFIG_COTTAGE_ESPNOW_KEY);
#endif
    nvs_handle_t nvs;
    if(nvs_open("room_config",NVS_READWRITE,&nvs)!=ESP_OK)abort();
    settings_t *saved=calloc(1,sizeof(*saved));size_t size=sizeof(*saved);
    if(saved&&nvs_get_blob(nvs,"profile",saved,&size)==ESP_OK){
        if(size==sizeof(current)&&saved->magic==SETTINGS_MAGIC){
            /* Never accept a persisted unterminated or invalid profile. */
            if(memchr(saved->ha,0,sizeof(saved->ha))&&memchr(saved->token,0,sizeof(saved->token))&&
               memchr(saved->controller,0,sizeof(saved->controller))&&memchr(saved->controller_token,0,sizeof(saved->controller_token))&&
               memchr(saved->ship_url,0,sizeof(saved->ship_url))&&cottage_integration_terminated(&saved->integrations))current=*saved;
        }else if(size==sizeof(old_settings_t)&&saved->magic==OLD_MAGIC){
            old_settings_t *old=(old_settings_t*)saved;
            if(memchr(old->ha,0,sizeof(old->ha))&&memchr(old->token,0,sizeof(old->token))&&memchr(old->controller,0,sizeof(old->controller))){
                memcpy(current.ha,old->ha,sizeof(current.ha));memcpy(current.token,old->token,sizeof(current.token));memcpy(current.controller,old->controller,sizeof(current.controller));
            }
        }
    }
    if(saved){memset(saved,0,sizeof(*saved));free(saved);}
    nvs_close(nvs);
}
bool room_web_authorized(httpd_req_t *r,bool mutation)
{
    if(mutation){
        char host[160],origin[180],expected[180];
        if(httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK||strpbrk(host,"/@?#\\ \r\n\t"))return false;
        snprintf(expected,sizeof(expected),"http://%s",host);
        /* Same-origin browser requests only. No cookie auth or permissive CORS. */
        if(httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK)return false;
        if(strcmp(expected,origin)){snprintf(expected,sizeof(expected),"https://%s",host);if(strcmp(expected,origin))return false;}
    }
    return true;
}
static esp_err_t json_reply(httpd_req_t *r,int code,cJSON *j)
{
    char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!s)return ESP_ERR_NO_MEM;
    if(code!=200)httpd_resp_set_status(r,code==400?"400 Bad Request":code==403?"403 Forbidden":code==409?"409 Conflict":"500 Internal Server Error");
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(r,s);free(s);return err;
}
static esp_err_t reply(httpd_req_t *r,int code,const char *message)
{cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"message",message);return json_reply(r,code,j);}
static esp_err_t page(httpd_req_t *r)
{
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-ancestors 'none'; object-src 'none'; base-uri 'none'");
    return httpd_resp_send(r,(const char*)settings_start,settings_end-settings_start-1);
}
static cJSON *sanitized(void)
{
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"ha_url",current.ha);cJSON_AddBoolToObject(j,"token_configured",*current.token!=0);
    cJSON_AddStringToObject(j,"controller",current.controller);cJSON_AddBoolToObject(j,"controller_token_configured",*current.controller_token!=0);
    cJSON_AddBoolToObject(j,"ships_enabled",current.ships_enabled);cJSON_AddStringToObject(j,"ship_url",current.ship_url);cJSON_AddNumberToObject(j,"ship_range",current.ship_range);
    const cottage_integration_t *p=&current.integrations;cJSON *m=cJSON_AddObjectToObject(j,"mqtt");
    cJSON_AddBoolToObject(m,"enabled",p->mqtt_enabled);cJSON_AddBoolToObject(m,"managed",current.mqtt_managed);
    cJSON_AddStringToObject(m,"broker",p->broker);cJSON_AddStringToObject(m,"username",p->username);
    cJSON_AddBoolToObject(m,"password_configured",*p->password!=0);cJSON_AddStringToObject(m,"prefix",p->prefix);cJSON_AddStringToObject(m,"name",p->name);
    cJSON *radio=cJSON_AddObjectToObject(j,"radio");cJSON_AddBoolToObject(radio,"enabled",p->radio_enabled);
    cJSON_AddStringToObject(radio,"peer",p->peer);cJSON_AddBoolToObject(radio,"key_configured",*p->key!=0);
    cJSON_AddNumberToObject(j,"volume",room_audio_get_volume());cJSON_AddBoolToObject(j,"camera_fill",room_display_camera_fill());return j;
}
static esp_err_t get(httpd_req_t *r)
{
    return json_reply(r,200,sanitized());
}
static void reboot_task(void *arg){vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();}
bool room_settings_reboot_later(void)
{
    if(reboot_pending)return true;
    reboot_pending=xTaskCreate(reboot_task,"settings_reboot",2048,NULL,3,NULL)==pdPASS;
    return reboot_pending;
}
static esp_err_t save(httpd_req_t *r)
{
    if(!room_web_authorized(r,true))return reply(r,403,"Use this device's own webpage to change settings.");
    if(reboot_pending||room_ota_busy())return reply(r,409,"A restart or firmware update is in progress.");
    char type[64];if(httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK||strcmp(type,"application/json"))return reply(r,400,"Use application/json.");
    if(r->content_len<=0||r->content_len>4096)return reply(r,400,"Invalid settings size.");
    char *body=calloc(1,r->content_len+1);if(!body)return ESP_ERR_NO_MEM;
    int at=0;while(at<r->content_len){int n=httpd_req_recv(r,body+at,r->content_len-at);if(n<=0){memset(body,0,r->content_len+1);free(body);return ESP_FAIL;}at+=n;}
    cJSON *j=cJSON_ParseWithLengthOpts(body,at+1,NULL,true);memset(body,0,at+1);free(body);
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,"section");const char *section=cJSON_IsString(v)?v->valuestring:"";
    settings_t *next=malloc(sizeof(*next));if(!next){cJSON_Delete(j);return ESP_ERR_NO_MEM;}*next=current;
    bool valid=false,restart=true;
    if(!strcmp(section,"ha")){
        char url[256];bool clear=false;
        valid=cottage_string(j,"ha_url",url,sizeof(url),false)&&cottage_origin(next->ha,sizeof(next->ha),url,true)&&cottage_bool(j,"clear_token",&clear);
        cJSON *token=cJSON_GetObjectItemCaseSensitive(j,"ha_token");valid=valid&&cJSON_IsString(token)&&printable(token->valuestring,2048)&&!(clear&&*token->valuestring);
        if(valid){if(clear)next->token[0]=0;else if(*token->valuestring)snprintf(next->token,sizeof(next->token),"%s",token->valuestring);}
    }else if(!strcmp(section,"controller")){
        char url[256];valid=cottage_string(j,"controller",url,sizeof(url),false)&&cottage_origin(next->controller,sizeof(next->controller),url,false);
        cJSON *token=cJSON_GetObjectItemCaseSensitive(j,"controller_token");valid=valid&&cJSON_IsString(token)&&(!*token->valuestring||(strlen(token->valuestring)==32&&printable(token->valuestring,32)));
        if(valid&&*token->valuestring)snprintf(next->controller_token,sizeof(next->controller_token),"%s",token->valuestring);
    }else if(!strcmp(section,"ships")){
        char url[256];cJSON *range=cJSON_GetObjectItemCaseSensitive(j,"range");
        valid=cottage_bool(j,"enabled",&next->ships_enabled)&&cottage_string(j,"url",url,sizeof(url),false)&&cottage_origin(next->ship_url,sizeof(next->ship_url),url,false)&&cJSON_IsNumber(range)&&
            (range->valuedouble==10||range->valuedouble==30||range->valuedouble==50||range->valuedouble==75||range->valuedouble==100);
        if(valid)next->ship_range=range->valueint;
    }else if(!strcmp(section,"mqtt")){
        valid=cottage_integration_patch(&next->integrations,j,false);
        if(valid){next->mqtt_managed=true;cJSON *password=cJSON_GetObjectItemCaseSensitive(j,"password");
            if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"clear_password"))){next->mqtt_password_known=true;next->mqtt_clear_password=true;}
            else if(cJSON_IsString(password)&&*password->valuestring){next->mqtt_password_known=true;next->mqtt_clear_password=false;}
        }
    }else if(!strcmp(section,"radio")){
        valid=cottage_integration_patch(&next->integrations,j,true);if(valid)next->radio_managed=true;
    }else if(!strcmp(section,"sound")){
        cJSON *volume=cJSON_GetObjectItemCaseSensitive(j,"volume"),*fill=cJSON_GetObjectItemCaseSensitive(j,"camera_fill");
        valid=cJSON_IsNumber(volume)&&volume->valuedouble>=0&&volume->valuedouble<=100&&volume->valueint==volume->valuedouble&&cJSON_IsBool(fill);
        if(valid)valid=room_audio_set_volume(volume->valueint);
        if(valid)room_display_set_camera_fill(cJSON_IsTrue(fill));
        restart=false;
    }else if(!strcmp(section,"sound_test")){valid=room_audio_test();restart=false;
    }else if(!strcmp(section,"wifi")){
        cJSON *ssid=cJSON_GetObjectItemCaseSensitive(j,"ssid"),*pw=cJSON_GetObjectItemCaseSensitive(j,"password"),*open=cJSON_GetObjectItemCaseSensitive(j,"open");
        valid=cJSON_IsString(ssid)&&cJSON_IsString(pw)&&cJSON_IsBool(open)&&room_wifi_submit(ssid->valuestring,pw->valuestring,cJSON_IsTrue(open));restart=false;
    }else if(!strcmp(section,"mqtt_test")){
        cJSON *status=room_controller_status();valid=status!=NULL;cJSON_Delete(status);restart=false;
    }
    cJSON_Delete(j);
    if(!valid){memset(next,0,sizeof(*next));free(next);return reply(r,400,"Settings invalid or device busy. Check the fields; no configuration was saved.");}
    if(restart){
        nvs_handle_t nvs;esp_err_t err=nvs_open("room_config",NVS_READWRITE,&nvs);
        if(err==ESP_OK){err=nvs_set_blob(nvs,"profile",next,sizeof(*next));if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}
        memset(next,0,sizeof(*next));free(next);
        if(err!=ESP_OK)return reply(r,500,"Could not save. Previous settings remain active.");
        bool scheduled=room_settings_reboot_later();return reply(r,200,scheduled?"Saved. Display restarting. Integration settings will be applied when the controller is reachable; its restart clears bench arming.":"Saved. Restart the display manually to apply settings.");
    }
    memset(next,0,sizeof(*next));free(next);return reply(r,200,"Request accepted. Live status reflects the result.");
}
cJSON *room_settings_controller_integrations(void)
{
    cJSON *j=cJSON_CreateObject();const cottage_integration_t *p=&current.integrations;
    if(current.mqtt_managed){cJSON *m=cJSON_AddObjectToObject(j,"mqtt");cJSON_AddBoolToObject(m,"enabled",p->mqtt_enabled);cJSON_AddStringToObject(m,"broker",p->broker);cJSON_AddStringToObject(m,"username",p->username);if(current.mqtt_password_known){cJSON_AddStringToObject(m,"password",p->password);cJSON_AddBoolToObject(m,"clear_password",current.mqtt_clear_password);}cJSON_AddStringToObject(m,"prefix",p->prefix);cJSON_AddStringToObject(m,"name",p->name);}
    if(current.radio_managed){
        uint8_t mac[6];char peer[18];esp_wifi_get_mac(WIFI_IF_STA,mac);
        snprintf(peer,sizeof(peer),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
        cJSON *m=cJSON_AddObjectToObject(j,"radio");cJSON_AddBoolToObject(m,"enabled",p->radio_enabled);cJSON_AddStringToObject(m,"peer",peer);cJSON_AddStringToObject(m,"key",p->key);
    }
    return j;
}
static esp_err_t status(httpd_req_t *r)
{
    cJSON *j=room_display_status();cJSON_AddItemToObject(j,"wifi",room_wifi_status());cJSON_AddItemToObject(j,"firmware",room_ota_status());
    return json_reply(r,200,j);
}
void room_settings_register(httpd_handle_t server)
{
    const httpd_uri_t routes[]={
        {.uri="/settings",.method=HTTP_GET,.handler=page},
        {.uri="/api/settings",.method=HTTP_GET,.handler=get},
        {.uri="/api/settings",.method=HTTP_POST,.handler=save},
        {.uri="/api/device/status",.method=HTTP_GET,.handler=status}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}

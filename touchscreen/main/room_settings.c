/* Settings are immutable for this boot; a saved profile applies after reboot.
 * This also keeps persistent HTTP sessions pinned to one origin. */
#include "room_settings.h"
#include "room_secrets.h"
#include "wifi_setup.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#define SETTINGS_MAGIC 0x43544731U
typedef struct {uint32_t magic;char ha[256],token[2049],controller[256];} settings_t;
static settings_t current;
static bool reboot_pending;
extern const unsigned char settings_start[] asm("_binary_settings_html_start");
extern const unsigned char settings_end[] asm("_binary_settings_html_end");
static bool endpoint(char *out,size_t capacity,const char *input,bool ha)
{
    size_t n=strlen(input);
    if(!n||n>=capacity)return false;
    char normalized[256];memcpy(normalized,input,n+1);
    while(n&&normalized[n-1]=='/')normalized[--n]=0;
    const char *host=normalized;bool scheme=false;
    if(!strncmp(host,"https://",8)){host+=8;scheme=true;}
    else if(!ha&&!strncmp(host,"http://",7)){host+=7;scheme=true;}
    else if(ha)return false;
    if(!*host||strpbrk(host,"/@?#\\ \r\n\t"))return false;
    size_t prefix=scheme?0:7;
    if(prefix+n>=capacity)return false;
    if(prefix)memcpy(out,"http://",prefix);
    memcpy(out+prefix,normalized,n+1);return true;
}
static bool token_valid(const char *token)
{
    if(strlen(token)>2048)return false;
    for(const unsigned char *p=(const unsigned char*)token;*p;++p)if(*p<33||*p>126)return false;
    return true;
}
const char *room_settings_ha_url(void){return current.ha;}
const char *room_settings_ha_token(void){return current.token;}
const char *room_settings_controller_url(void){return current.controller;}
void room_settings_init(void)
{
    current.magic=SETTINGS_MAGIC;
    snprintf(current.ha,sizeof(current.ha),"%s",COTTAGE_HA_URL);
    snprintf(current.token,sizeof(current.token),"%s",COTTAGE_HA_TOKEN);
    snprintf(current.controller,sizeof(current.controller),"%s",CONFIG_ROOM_CONTROLLER_URL);
    nvs_handle_t nvs;
    if(nvs_open("room_config",NVS_READONLY,&nvs)!=ESP_OK)return;
    settings_t *saved=malloc(sizeof(*saved));size_t size=sizeof(*saved);
    if(saved&&nvs_get_blob(nvs,"profile",saved,&size)==ESP_OK&&size==sizeof(*saved)&&saved->magic==SETTINGS_MAGIC){
        saved->ha[sizeof(saved->ha)-1]=0;saved->token[sizeof(saved->token)-1]=0;saved->controller[sizeof(saved->controller)-1]=0;
        char ha[256],controller[256];
        if(endpoint(ha,sizeof(ha),saved->ha,true)&&endpoint(controller,sizeof(controller),saved->controller,false)&&token_valid(saved->token)){
            current=*saved;
            snprintf(current.ha,sizeof(current.ha),"%s",ha);
            snprintf(current.controller,sizeof(current.controller),"%s",controller);
        }
    }
    if(saved){memset(saved,0,sizeof(*saved));free(saved);}nvs_close(nvs);
}
static esp_err_t reply(httpd_req_t *r,int code,const char *message)
{
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"message",message);
    char *body=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!body)return ESP_ERR_NO_MEM;
    if(code!=200)httpd_resp_set_status(r,code==400?"400 Bad Request":code==403?"403 Forbidden":code==409?"409 Conflict":"500 Internal Server Error");
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(r,body);free(body);return err;
}
static esp_err_t settings_page(httpd_req_t *r)
{
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; frame-ancestors 'none'");
    return httpd_resp_send(r,(const char*)settings_start,settings_end-settings_start-1);
}
static esp_err_t settings_get(httpd_req_t *r)
{
    bool editable=room_wifi_setup_request(r);
    if(!editable)return reply(r,403,"Tap Wi-Fi on the device, join its setup network, then open http://192.168.4.1/settings");
    cJSON *j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"ha_url",current.ha);cJSON_AddStringToObject(j,"controller",current.controller);
    cJSON_AddBoolToObject(j,"token_configured",current.token[0]!=0);
    char *body=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!body)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(r,body);free(body);return err;
}
static void reboot_task(void *arg){vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();}
static esp_err_t settings_save(httpd_req_t *r)
{
    char marker[8],origin[80],host[64],type[64];
    if(!room_wifi_setup_request(r)||
       httpd_req_get_hdr_value_str(r,"X-Cottage-Setup",marker,sizeof(marker))!=ESP_OK||strcmp(marker,"1")||
       httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK||(strcmp(host,"192.168.4.1")&&strcmp(host,"192.168.4.1:80"))||
       httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK||strcmp(type,"application/json")||
       (httpd_req_get_hdr_value_len(r,"Origin")&&(httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK||strcmp(origin,"http://192.168.4.1"))))
        return reply(r,403,"Join the device setup Wi-Fi to change settings.");
    if(reboot_pending)return reply(r,409,"Settings already saved; reboot in progress.");
    if(r->content_len<=0||r->content_len>4096)return reply(r,400,"Invalid settings size.");
    char *body=malloc(r->content_len+1);if(!body)return ESP_ERR_NO_MEM;
    int at=0;while(at<r->content_len){int n=httpd_req_recv(r,body+at,r->content_len-at);if(n<=0){memset(body,0,r->content_len+1);free(body);return ESP_FAIL;}at+=n;}body[at]=0;
    cJSON *j=cJSON_ParseWithLengthOpts(body,at+1,NULL,true);memset(body,0,at+1);free(body);
    cJSON *ha=cJSON_GetObjectItemCaseSensitive(j,"ha_url"),*controller=cJSON_GetObjectItemCaseSensitive(j,"controller"),
        *token=cJSON_GetObjectItemCaseSensitive(j,"ha_token"),*clear=cJSON_GetObjectItemCaseSensitive(j,"clear_token");
    settings_t *next=malloc(sizeof(*next));if(!next){cJSON_Delete(j);return ESP_ERR_NO_MEM;}*next=current;
    bool valid=cJSON_IsObject(j)&&cJSON_GetArraySize(j)==4&&cJSON_IsString(ha)&&cJSON_IsString(controller)&&cJSON_IsString(token)&&cJSON_IsBool(clear);
    if(valid)valid=endpoint(next->ha,sizeof(next->ha),ha->valuestring,true)&&endpoint(next->controller,sizeof(next->controller),controller->valuestring,false)&&token_valid(token->valuestring)&&!(cJSON_IsTrue(clear)&&*token->valuestring);
    if(valid){
        if(cJSON_IsTrue(clear))next->token[0]=0;
        else if(*token->valuestring)snprintf(next->token,sizeof(next->token),"%s",token->valuestring);
    }
    /* Wipe token text before the JSON tree is freed. It is never echoed/logged. */
    if(cJSON_IsString(token))memset(token->valuestring,0,strlen(token->valuestring));
    cJSON_Delete(j);
    if(!valid){memset(next,0,sizeof(*next));free(next);return reply(r,400,"Use an HTTPS HA origin, a controller IP/name or origin, and a valid token. Do not paste a token while selecting Clear.");}
    nvs_handle_t nvs;esp_err_t err=nvs_open("room_config",NVS_READWRITE,&nvs);
    if(err==ESP_OK){err=nvs_set_blob(nvs,"profile",next,sizeof(*next));if(err==ESP_OK)err=nvs_commit(nvs);nvs_close(nvs);}
    memset(next,0,sizeof(*next));free(next);
    if(err!=ESP_OK)return reply(r,500,"Could not save settings; previous settings remain active.");
    reboot_pending=true;
    bool restarting=xTaskCreate(reboot_task,"settings_reboot",2048,NULL,3,NULL)==pdPASS;
    return reply(r,200,restarting?"Settings saved. Device is restarting; reconnect to your normal Wi-Fi.":"Settings saved. Restart the device to apply.");
}
void room_settings_register(httpd_handle_t server)
{
    const httpd_uri_t routes[]={
        {.uri="/settings",.method=HTTP_GET,.handler=settings_page},
        {.uri="/api/settings",.method=HTTP_GET,.handler=settings_get},
        {.uri="/api/settings",.method=HTTP_POST,.handler=settings_save}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}

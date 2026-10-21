/* Persistent Wi-Fi provisioning. Only AP clients can change network settings.
 * HTTP, DNS and reconnect work never runs on the LVGL task. */
#include "wifi_setup.h"
#include "wifi_portal_helpers.h"
#include "thermo_radio.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "dhcpserver/dhcpserver.h"

#define PROFILE_MAGIC 0x43575031u
#define DISCONNECTED_BIT 1
#define SETUP_URL "http://192.168.4.1/wifi"
static const char *TAG="room_wifi";
typedef struct {uint32_t magic;char ssid[33],password[65];} profile_t;
typedef struct {
    bool connected,portal,pending,saved,connecting;
    unsigned reason;
    int64_t offline_at,next_try;
    char ip[16],message[140];
} state_t;
static state_t state;
static profile_t current_profile,saved_profile;
static SemaphoreHandle_t state_mutex,action_mutex;
static QueueHandle_t profile_queue;
static EventGroupHandle_t wifi_events;
static esp_netif_t *ap_netif;
static char setup_ssid[33];
static atomic_bool setup_requested;
extern const unsigned char wifi_page_start[] asm("_binary_wifi_setup_html_start");
extern const unsigned char wifi_page_end[] asm("_binary_wifi_setup_html_end");

static state_t get_state(void)
{
    state_t result;xSemaphoreTake(state_mutex,portMAX_DELAY);result=state;xSemaphoreGive(state_mutex);return result;
}
bool room_wifi_connected(void){return state_mutex&&get_state().connected;}
static void set_message(const char *message)
{
    xSemaphoreTake(state_mutex,portMAX_DELAY);snprintf(state.message,sizeof(state.message),"%s",message);xSemaphoreGive(state_mutex);
}
static bool load_profile(profile_t *p)
{
    nvs_handle_t h;size_t length=sizeof(*p);esp_err_t err=nvs_open("room_wifi",NVS_READONLY,&h);
    if(err!=ESP_OK)return false;
    err=nvs_get_blob(h,"profile",p,&length);nvs_close(h);
    return err==ESP_OK&&length==sizeof(*p)&&p->magic==PROFILE_MAGIC&&
        memchr(p->ssid,0,sizeof(p->ssid))&&memchr(p->password,0,sizeof(p->password))&&portal_profile_valid(p->ssid,p->password);
}
static esp_err_t save_profile(profile_t *p)
{
    nvs_handle_t h;esp_err_t err=nvs_open("room_wifi",NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=nvs_set_blob(h,"profile",p,sizeof(*p));if(err==ESP_OK)err=nvs_commit(h);nvs_close(h);return err;
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){
        wifi_event_sta_disconnected_t *e=data;int64_t now=esp_timer_get_time();
        xSemaphoreTake(state_mutex,portMAX_DELAY);
        state.connected=false;state.connecting=false;state.reason=e->reason;state.ip[0]=0;
        if(!state.offline_at)state.offline_at=now;
        state.next_try=now+(tl_radio_configured()?20000000:2000000);
        xSemaphoreGive(state_mutex);xEventGroupSetBits(wifi_events,DISCONNECTED_BIT);
        ESP_LOGW(TAG,"Wi-Fi disconnected: reason=%u",(unsigned)e->reason);
    }
    if(base==IP_EVENT&&id==IP_EVENT_STA_LOST_IP){
        xSemaphoreTake(state_mutex,portMAX_DELAY);
        state.connected=false;state.connecting=true;state.ip[0]=0;
        if(!state.offline_at)state.offline_at=esp_timer_get_time();
        xSemaphoreGive(state_mutex);
        ESP_LOGW(TAG,"Wi-Fi station lost its IP address");
    }
    if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *e=data;
        xSemaphoreTake(state_mutex,portMAX_DELAY);
        if(!state.connecting&&!state.connected){xSemaphoreGive(state_mutex);return;}
        state.connected=true;state.connecting=false;state.reason=0;state.offline_at=0;
        snprintf(state.ip,sizeof(state.ip),IPSTR,IP2STR(&e->ip_info.ip));
        xSemaphoreGive(state_mutex);
        ESP_LOGI(TAG,"Room UI: http://" IPSTR,IP2STR(&e->ip_info.ip));
    }
}
static bool apply_station(const profile_t *p)
{
    xSemaphoreTake(state_mutex,portMAX_DELAY);state.connected=false;state.connecting=false;state.ip[0]=0;xSemaphoreGive(state_mutex);
    if(xSemaphoreTake(action_mutex,pdMS_TO_TICKS(1000))!=pdTRUE)return false;
    xEventGroupClearBits(wifi_events,DISCONNECTED_BIT);
    esp_err_t disconnected=esp_wifi_disconnect();
    if(disconnected==ESP_OK)xEventGroupWaitBits(wifi_events,DISCONNECTED_BIT,pdTRUE,pdFALSE,pdMS_TO_TICKS(1000));
    wifi_config_t cfg={0};
    memcpy(cfg.sta.ssid,p->ssid,strlen(p->ssid));memcpy(cfg.sta.password,p->password,strlen(p->password));
    cfg.sta.threshold.authmode=*p->password?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;
    cfg.sta.pmf_cfg.capable=true;cfg.sta.pmf_cfg.required=false;
    esp_err_t err=esp_wifi_set_config(WIFI_IF_STA,&cfg);
    memset(&cfg,0,sizeof(cfg));
    xSemaphoreTake(state_mutex,portMAX_DELAY);
    state.connected=false;state.connecting=err==ESP_OK;state.ip[0]=0;
    if(!state.offline_at)state.offline_at=esp_timer_get_time();
    xSemaphoreGive(state_mutex);
    bool applied=err==ESP_OK;
    if(applied)err=esp_wifi_connect();
    if(err!=ESP_OK){
        xSemaphoreTake(state_mutex,portMAX_DELAY);state.connecting=false;state.next_try=esp_timer_get_time()+2000000;xSemaphoreGive(state_mutex);
    }
    xSemaphoreGive(action_mutex);return applied;
}
static void set_portal(bool enabled)
{
    if(xSemaphoreTake(action_mutex,pdMS_TO_TICKS(1000))!=pdTRUE)return;
    esp_err_t err=esp_wifi_set_mode(enabled?WIFI_MODE_APSTA:WIFI_MODE_STA);
    xSemaphoreGive(action_mutex);
    if(err!=ESP_OK){ESP_LOGW(TAG,"Could not change setup AP mode: %s",esp_err_to_name(err));return;}
    xSemaphoreTake(state_mutex,portMAX_DELAY);state.portal=enabled;xSemaphoreGive(state_mutex);
    if(enabled)ESP_LOGI(TAG,"Wi-Fi setup: join %s, open " SETUP_URL,setup_ssid);
    else ESP_LOGI(TAG,"Wi-Fi setup network closed");
}
void room_wifi_request_setup(void){setup_requested=true;}
bool room_wifi_setup_hint(char *buffer,size_t size)
{
    state_t s=get_state();if(!s.portal)return false;
    snprintf(buffer,size,"Wi-Fi setup: %s / 192.168.4.1",setup_ssid);return true;
}
static void network_task(void *arg)
{
    bool configured=*current_profile.ssid,need_save=configured&&!saved_profile.magic,testing=false;
    bool was_connected=false,profile_applied=false;int64_t tested_at=0,close_at=0,connect_started=0;
    if(!configured){set_message("Choose a network and enter its password.");set_portal(true);}
    else {profile_applied=apply_station(&current_profile);connect_started=esp_timer_get_time();}
    for(;;){
        int64_t now=esp_timer_get_time();profile_t candidate;
        if(xQueueReceive(profile_queue,&candidate,0)==pdTRUE){
            current_profile=candidate;memset(&candidate,0,sizeof(candidate));
            configured=true;need_save=true;testing=true;tested_at=now;was_connected=false;close_at=0;
            if(!get_state().portal)set_portal(true);
            profile_applied=apply_station(&current_profile);
            if(!profile_applied){
                xSemaphoreTake(state_mutex,portMAX_DELAY);state.connecting=false;state.next_try=now+(tl_radio_configured()?20000000:2000000);xSemaphoreGive(state_mutex);
            }
            connect_started=now;
        }
        state_t s=get_state();
        if(atomic_exchange(&setup_requested,false)){
            if(!s.portal)set_portal(true);
            if(!s.pending)set_message("Choose a network. Existing saved settings remain until a new connection succeeds.");
            close_at=s.connected?now+300000000:0;
        }
        s=get_state();
        if(s.connected){
            if(need_save){
                esp_err_t err=save_profile(&current_profile);
                xSemaphoreTake(state_mutex,portMAX_DELAY);
                state.pending=false;state.saved=err==ESP_OK;
                snprintf(state.message,sizeof(state.message),"%s",err==ESP_OK?
                    "Connected and saved. Rejoin your normal Wi-Fi; this setup network closes in 30 seconds.":
                    "Connected, but settings could not be saved. Submit again to retry.");
                xSemaphoreGive(state_mutex);
                if(err==ESP_OK){saved_profile=current_profile;close_at=now+30000000;}
                need_save=false;testing=false;
            }else if(!was_connected&&s.portal&&!close_at)close_at=now+300000000;
            if(s.portal&&close_at&&now>=close_at)set_portal(false);
            was_connected=true;
        }else{
            was_connected=false;close_at=0;
            if(!s.connecting&&!testing)tl_radio_search_tick();
            if(!s.portal&&s.offline_at&&now-s.offline_at>=30000000){
                set_message("Wi-Fi unavailable. Select a network or check its password.");set_portal(true);
            }
            if(testing&&now-tested_at>=30000000){
                testing=false;need_save=false;
                xSemaphoreTake(state_mutex,portMAX_DELAY);state.pending=false;state.saved=false;
                snprintf(state.message,sizeof(state.message),"Could not connect. Previous saved settings were retained. Check the network/password and retry.");
                xSemaphoreGive(state_mutex);
                current_profile=saved_profile;configured=*current_profile.ssid;
                profile_applied=configured&&apply_station(&current_profile);
                if(!configured&&xSemaphoreTake(action_mutex,pdMS_TO_TICKS(1000))==pdTRUE){esp_wifi_disconnect();xSemaphoreGive(action_mutex);}
                connect_started=now;
            }else if(configured&&now>=s.next_try){
                if(!profile_applied){
                    profile_applied=apply_station(&current_profile);connect_started=now;
                    vTaskDelay(pdMS_TO_TICKS(250));continue;
                }
                /* A driver can remain in authentication without producing an IP. Bound the attempt. */
                if(!s.connecting||(connect_started&&now-connect_started>=(tl_radio_configured()?5000000:15000000))){
                    if(xSemaphoreTake(action_mutex,0)==pdTRUE){
                        if(s.connecting){ESP_LOGW(TAG,"Wi-Fi attempt timed out without an IP address");esp_wifi_disconnect();}
                        else{
                            esp_err_t err=esp_wifi_connect();
                            xSemaphoreTake(state_mutex,portMAX_DELAY);state.connecting=err==ESP_OK;state.next_try=now+(tl_radio_configured()?20000000:2000000);xSemaphoreGive(state_mutex);
                            connect_started=now;
                        }
                        xSemaphoreGive(action_mutex);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
static void dns_task(void *arg)
{
    for(;;){
        if(!get_state().portal){vTaskDelay(pdMS_TO_TICKS(500));continue;}
        int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(fd<0){vTaskDelay(pdMS_TO_TICKS(1000));continue;}
        struct timeval timeout={.tv_sec=1};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        struct sockaddr_in local={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=inet_addr("192.168.4.1")};
        if(bind(fd,(struct sockaddr*)&local,sizeof(local))<0){close(fd);vTaskDelay(pdMS_TO_TICKS(1000));continue;}
        while(get_state().portal){
            uint8_t packet[512];struct sockaddr_in peer;socklen_t size=sizeof(peer);
            int n=recvfrom(fd,packet,sizeof(packet),0,(struct sockaddr*)&peer,&size);
            if(n>0){size_t length=portal_dns_reply(packet,n,sizeof(packet));if(length)sendto(fd,packet,length,0,(struct sockaddr*)&peer,size);}
        }
        close(fd);
    }
}
void room_wifi_start(void)
{
    state_mutex=xSemaphoreCreateMutex();action_mutex=xSemaphoreCreateMutex();profile_queue=xQueueCreate(1,sizeof(profile_t));wifi_events=xEventGroupCreate();
    ESP_ERROR_CHECK(state_mutex&&action_mutex&&profile_queue&&wifi_events?ESP_OK:ESP_ERR_NO_MEM);
    state.offline_at=esp_timer_get_time();
    bool stored=load_profile(&saved_profile);
    if(stored){current_profile=saved_profile;state.saved=true;}
    else{
        memset(&saved_profile,0,sizeof(saved_profile));
        /* One-time migration for an existing menuconfig network; blank/default starts setup immediately. */
        if(strcmp(CONFIG_ROOM_WIFI_SSID,"CottageThermoBench")&&portal_profile_valid(CONFIG_ROOM_WIFI_SSID,CONFIG_ROOM_WIFI_PASSWORD)){
            current_profile.magic=PROFILE_MAGIC;snprintf(current_profile.ssid,sizeof(current_profile.ssid),"%s",CONFIG_ROOM_WIFI_SSID);
            snprintf(current_profile.password,sizeof(current_profile.password),"%s",CONFIG_ROOM_WIFI_PASSWORD);
        }
    }
    esp_netif_t *sta=esp_netif_create_default_wifi_sta();ap_netif=esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(sta&&ap_netif?ESP_OK:ESP_FAIL);ESP_ERROR_CHECK(esp_netif_set_hostname(sta,"CottageThermostat"));
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
    snprintf(setup_ssid,sizeof(setup_ssid),"CottageThermo-%02X%02X-Setup",mac[4],mac[5]);
    wifi_config_t ap={0};
    size_t setup_length=strlen(setup_ssid);
    ESP_ERROR_CHECK(setup_length<=sizeof(ap.ap.ssid)?ESP_OK:ESP_ERR_INVALID_SIZE);
    /* ESP-IDF stores an SSID as up to 32 bytes plus its explicit length. */
    memcpy(ap.ap.ssid,setup_ssid,setup_length);
    ap.ap.ssid_len=setup_length;ap.ap.authmode=WIFI_AUTH_OPEN;ap.ap.channel=1;ap.ap.max_connection=4;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    esp_netif_ip_info_t ip={0};ip.ip.addr=inet_addr("192.168.4.1");ip.gw=ip.ip;ip.netmask.addr=inet_addr("255.255.255.0");
    esp_netif_dhcps_stop(ap_netif);ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif,&ip));
    uint8_t offer=OFFER_DNS;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer,sizeof(offer)));
    esp_netif_dns_info_t dns={0};dns.ip.type=ESP_IPADDR_TYPE_V4;dns.ip.u_addr.ip4=ip.ip;
    ESP_ERROR_CHECK(esp_netif_set_dns_info(ap_netif,ESP_NETIF_DNS_MAIN,&dns));
    static char uri[]=SETUP_URL;
    ESP_ERROR_CHECK(esp_netif_dhcps_option(ap_netif,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,uri,strlen(uri)));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
    ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    set_message(stored?"Connecting to saved Wi-Fi...":"Waiting for Wi-Fi setup...");
    ESP_ERROR_CHECK(xTaskCreate(network_task,"wifi_setup",6144,NULL,4,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(dns_task,"setup_dns",4096,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}
bool room_wifi_setup_request(httpd_req_t *r)
{
    if(!get_state().portal)return false;
    struct sockaddr_storage address={0};socklen_t length=sizeof(address);
    if(getsockname(httpd_req_to_sockfd(r),(struct sockaddr*)&address,&length)!=0)return false;
    if(address.ss_family==AF_INET&&length>=sizeof(struct sockaddr_in)){
        const struct sockaddr_in *v4=(const struct sockaddr_in*)&address;
        return portal_setup_address((const uint8_t*)&v4->sin_addr,4);
    }
#if CONFIG_LWIP_IPV6
    if(address.ss_family==AF_INET6&&length>=sizeof(struct sockaddr_in6)){
        const struct sockaddr_in6 *v6=(const struct sockaddr_in6*)&address;
        return portal_setup_address((const uint8_t*)&v6->sin6_addr,16);
    }
#endif
    return false;
}
static esp_err_t json_reply(httpd_req_t *r,int code,cJSON *j)
{
    char *text=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!text)return ESP_ERR_NO_MEM;
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    if(code!=200)httpd_resp_set_status(r,code==400?"400 Bad Request":code==403?"403 Forbidden":code==409?"409 Conflict":"503 Service Unavailable");
    esp_err_t err=httpd_resp_sendstr(r,text);free(text);return err;
}
static esp_err_t error_reply(httpd_req_t *r,int code,const char *text)
{cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"error",text);return json_reply(r,code,j);}
esp_err_t room_wifi_setup_get(httpd_req_t *r)
{
    if(!room_wifi_setup_request(r))return error_reply(r,403,"Join the setup Wi-Fi network to configure this device.");
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");
    httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; frame-ancestors 'none'; form-action 'self'");
    return httpd_resp_send(r,(const char*)wifi_page_start,wifi_page_end-wifi_page_start-1);
}
static esp_err_t status_get(httpd_req_t *r)
{
    if(!room_wifi_setup_request(r))return error_reply(r,403,"Setup is available only on the setup Wi-Fi network.");
    state_t s=get_state();cJSON *j=cJSON_CreateObject();
    cJSON_AddBoolToObject(j,"connected",s.connected);cJSON_AddBoolToObject(j,"pending",s.pending);cJSON_AddBoolToObject(j,"saved",s.saved);
    cJSON_AddNumberToObject(j,"reason",s.reason);cJSON_AddStringToObject(j,"message",s.message);cJSON_AddStringToObject(j,"ip",s.ip);
    return json_reply(r,200,j);
}
static esp_err_t scan_get(httpd_req_t *r)
{
    if(!room_wifi_setup_request(r))return error_reply(r,403,"Join the setup Wi-Fi network first.");
    if(get_state().pending||xSemaphoreTake(action_mutex,0)!=pdTRUE)return error_reply(r,409,"Connection test in progress. Wait or enter the network name manually.");
    wifi_scan_config_t cfg={.show_hidden=false};
    esp_err_t err=esp_wifi_scan_start(&cfg,true);wifi_ap_record_t *records=NULL;uint16_t count=0;
    if(err==ESP_OK){esp_wifi_scan_get_ap_num(&count);if(count>24)count=24;
        if(count){records=calloc(count,sizeof(*records));if(records)err=esp_wifi_scan_get_ap_records(&count,records);else err=ESP_ERR_NO_MEM;}}
    esp_wifi_clear_ap_list();xSemaphoreGive(action_mutex);
    if(err!=ESP_OK){free(records);return error_reply(r,503,"Scan unavailable while connecting. Enter the SSID manually or retry.");}
    cJSON *j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"networks");
    for(int k=0;k<count&&records;++k){
        char ssid[33];memcpy(ssid,records[k].ssid,32);ssid[32]=0;if(!*ssid)continue;
        bool duplicate=false;for(int n=0;n<k;++n)if(!memcmp(records[k].ssid,records[n].ssid,32))duplicate=true;
        if(duplicate)continue;
        cJSON *entry=cJSON_CreateObject();cJSON_AddStringToObject(entry,"ssid",ssid);cJSON_AddNumberToObject(entry,"rssi",records[k].rssi);
        cJSON_AddBoolToObject(entry,"open",records[k].authmode==WIFI_AUTH_OPEN);cJSON_AddItemToArray(list,entry);
    }
    free(records);return json_reply(r,200,j);
}
cJSON *room_wifi_status(void)
{
    state_t s=get_state();cJSON *j=cJSON_CreateObject();
    cJSON_AddBoolToObject(j,"connected",s.connected);cJSON_AddBoolToObject(j,"portal",s.portal);
    cJSON_AddBoolToObject(j,"pending",s.pending);cJSON_AddStringToObject(j,"ip",s.ip);cJSON_AddStringToObject(j,"message",s.message);
    return j;
}
void room_wifi_address(char *buffer,size_t size){state_t s=get_state();snprintf(buffer,size,"%s",s.ip);}
bool room_wifi_submit(const char *ssid,const char *password,bool open)
{
    if(!portal_profile_valid(ssid,password)||((*password==0)!=open))return false;
    profile_t p={.magic=PROFILE_MAGIC};snprintf(p.ssid,sizeof(p.ssid),"%s",ssid);snprintf(p.password,sizeof(p.password),"%s",password);
    xSemaphoreTake(state_mutex,portMAX_DELAY);bool ok=!state.pending&&xQueueSend(profile_queue,&p,0)==pdTRUE;
    if(ok){state.pending=true;state.saved=false;snprintf(state.message,sizeof(state.message),"Testing Wi-Fi connection...");}
    xSemaphoreGive(state_mutex);memset(&p,0,sizeof(p));return ok;
}
static esp_err_t connect_post(httpd_req_t *r)
{
    if(!room_wifi_setup_request(r))return error_reply(r,403,"Join the setup Wi-Fi network first.");
    char marker[8],host[40],type[64],origin[64];
    if(httpd_req_get_hdr_value_str(r,"X-Cottage-Setup",marker,sizeof(marker))!=ESP_OK||strcmp(marker,"1")||
       httpd_req_get_hdr_value_str(r,"Host",host,sizeof(host))!=ESP_OK||(strcmp(host,"192.168.4.1")&&strcmp(host,"192.168.4.1:80"))||
       httpd_req_get_hdr_value_str(r,"Content-Type",type,sizeof(type))!=ESP_OK||strncmp(type,"application/json",16))
        return error_reply(r,403,"Open setup at http://192.168.4.1.");
    if(httpd_req_get_hdr_value_len(r,"Origin")&&
       (httpd_req_get_hdr_value_str(r,"Origin",origin,sizeof(origin))!=ESP_OK||strcmp(origin,"http://192.168.4.1")))
        return error_reply(r,403,"Setup origin rejected.");
    if(r->content_len<=0||r->content_len>=384)return error_reply(r,400,"Invalid setup body.");
    char body[384];int at=0;
    while(at<r->content_len){int n=httpd_req_recv(r,body+at,r->content_len-at);if(n<=0){memset(body,0,sizeof(body));return ESP_FAIL;}at+=n;}body[at]=0;
    cJSON *j=cJSON_ParseWithLengthOpts(body,at+1,NULL,true);memset(body,0,sizeof(body));
    cJSON *ssid=cJSON_GetObjectItemCaseSensitive(j,"ssid"),*password=cJSON_GetObjectItemCaseSensitive(j,"password"),*open=cJSON_GetObjectItemCaseSensitive(j,"open");
    bool valid=cJSON_IsObject(j)&&cJSON_GetArraySize(j)==3&&cJSON_IsString(ssid)&&cJSON_IsString(password)&&cJSON_IsBool(open)&&
        portal_profile_valid(ssid->valuestring,password->valuestring)&&((password->valuestring[0]=='\0')==cJSON_IsTrue(open));
    profile_t p={.magic=PROFILE_MAGIC};
    if(valid){snprintf(p.ssid,sizeof(p.ssid),"%s",ssid->valuestring);snprintf(p.password,sizeof(p.password),"%s",password->valuestring);}
    cJSON_Delete(j);
    if(!valid)return error_reply(r,400,"Use a 1-32 byte SSID and an 8-63 character password (or a 64-digit hex key). Select no-password only for open networks.");
    xSemaphoreTake(state_mutex,portMAX_DELAY);
    bool accepted=!state.pending&&xQueueSend(profile_queue,&p,0)==pdTRUE;
    if(accepted){state.pending=true;state.saved=false;snprintf(state.message,sizeof(state.message),"Testing Wi-Fi connection...");}
    xSemaphoreGive(state_mutex);memset(&p,0,sizeof(p));
    if(!accepted)return error_reply(r,409,"A connection test is already in progress.");
    cJSON *result=cJSON_CreateObject();cJSON_AddBoolToObject(result,"accepted",true);return json_reply(r,200,result);
}
static esp_err_t redirect_404(httpd_req_t *r,httpd_err_code_t error)
{
    if(room_wifi_setup_request(r)){
        httpd_resp_set_status(r,"303 See Other");httpd_resp_set_hdr(r,"Location",SETUP_URL);
        httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,"Open Wi-Fi setup at " SETUP_URL);
    }
    return httpd_resp_send_err(r,HTTPD_404_NOT_FOUND,"Not found");
}
void room_wifi_register(httpd_handle_t server)
{
    const httpd_uri_t routes[]={
        {.uri="/wifi",.method=HTTP_GET,.handler=room_wifi_setup_get},
        {.uri="/api/wifi/status",.method=HTTP_GET,.handler=status_get},
        {.uri="/api/wifi/scan",.method=HTTP_GET,.handler=scan_get},
        {.uri="/api/wifi/connect",.method=HTTP_POST,.handler=connect_post}};
    for(unsigned n=0;n<sizeof(routes)/sizeof(routes[0]);++n)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[n]));
    ESP_ERROR_CHECK(httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,redirect_404));
}

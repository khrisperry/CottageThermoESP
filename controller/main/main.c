/* Controller foundation. Automatic output defaults to simulation. */
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_sntp.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "thermo.h"
#include "thermo_radio.h"
#include "controller_mqtt.h"
#include "controller_encoder.h"
#include "controller_integrations.h"
#include <stdatomic.h>
#include "network_profile.h"
#include "controller_wifi_retry.h"
#include "controller_wifi_recovery.h"

static const char *TAG="thermostat";
#ifdef CONFIG_THERMO_LIVE_BENCH
#define CONFIG_KEY "bench_config"
#else
#define CONFIG_KEY "sim_config"
#endif
static th_state_t state;
static SemaphoreHandle_t lock;
static nvs_handle_t store;
static uint64_t controller_boot;
static tl_guard_t room_guard;
static atomic_bool station_up,station_connecting,setup_recovery;
static atomic_uint station_failures;
static network_profile_t network;
static esp_netif_t *station_netif;
static char station_mac[18];
static unsigned http_link_count;
static uint64_t last_http_link;
static atomic_uint reconnect_at,connect_at,setup_discovery_until;
static bool pwm, timer_ready, armed;
static uint64_t armed_until;
static int last_pulse=-1;
static char last_reason[64];
typedef struct { uint64_t ms; char text[96]; } event_t;
static event_t events[64];
static unsigned event_count;
typedef struct {
    uint64_t uptime_ms; time_t unix_s;
    float temperature_f,target_f,level;
    th_source_t source;
} history_t;
static history_t history[360];
static unsigned history_count;
extern const unsigned char ui_start[] asm("_binary_ui_html_start");
extern const unsigned char ui_end[] asm("_binary_ui_html_end");
static uint64_t now_ms(void) { return esp_timer_get_time()/1000; }
static void event(const char *text)
{
    event_t *e=&events[event_count++%64];
    e->ms=now_ms(); snprintf(e->text,sizeof(e->text),"%s",text);
    ESP_LOGI(TAG,"%s",text);
}
static esp_err_t pulse(int us)
{
    if (us<500 || us>2500) return ESP_ERR_INVALID_ARG;
    if (!timer_ready) {
        ledc_timer_config_t t={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_13_BIT,
            .timer_num=LEDC_TIMER_0,.freq_hz=50,.clk_cfg=LEDC_AUTO_CLK};
        esp_err_t err=ledc_timer_config(&t);
        if (err!=ESP_OK) return err;
        timer_ready=true;
    }
    ledc_channel_config_t c={.gpio_num=CONFIG_THERMO_SERVO_GPIO,.speed_mode=LEDC_LOW_SPEED_MODE,
        .channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=(uint32_t)((uint64_t)us*8192/20000)};
    esp_err_t err=ledc_channel_config(&c);
    if (err==ESP_OK) { pwm=true; last_pulse=us; }
    return err;
}
static void release(void)
{
    if (pwm) {
        esp_err_t err=ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);
        if (err!=ESP_OK) ESP_LOGE(TAG,"PWM stop: %s",esp_err_to_name(err));
    }
    pwm=false; armed=false; th_release(&state);
}
static esp_err_t json_send(httpd_req_t *r,cJSON *j)
{
    if (!j) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");
    char *s=cJSON_PrintUnformatted(j); cJSON_Delete(j);
    if (!s) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");
    httpd_resp_set_type(r,"application/json"); httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_sendstr(r,s); free(s); return err;
}
static cJSON *body(httpd_req_t *r)
{
    if (r->content_len<=0 || r->content_len>4096) return NULL;
    char *s=calloc(1,r->content_len+1); if (!s) return NULL;
    int at=0;
    while (at<r->content_len) {
        int n=httpd_req_recv(r,s+at,r->content_len-at);
        if (n<=0) {free(s);return NULL;} at+=n;
    }
    cJSON *j=cJSON_ParseWithLengthOpts(s,r->content_len+1,NULL,true); free(s);
    if (!cJSON_IsObject(j)) {cJSON_Delete(j);return NULL;} return j;
}
static bool number(cJSON *j,const char *key,double *out)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble)) return false;
    *out=v->valuedouble; return true;
}
static bool text_is(cJSON *j,const char *key,const char *s)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsString(v) && strcmp(v->valuestring,s)==0;
}
static esp_err_t root_get(httpd_req_t *r)
{
    httpd_resp_set_type(r,"text/html; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,(const char *)ui_start,ui_end-ui_start-1);
}
static cJSON *config_json(void)
{
    th_config_t *c=&state.config;
    cJSON *j=cJSON_CreateObject();
    cJSON_AddNumberToObject(j,"schema",c->schema);
    cJSON_AddStringToObject(j,"mode",th_mode_name(c->mode));
    cJSON_AddNumberToObject(j,"target_f",c->target_f);
    cJSON_AddNumberToObject(j,"direct_level",c->direct_level);
    cJSON_AddBoolToObject(j,"secondary_enabled",c->secondary_enabled);
    cJSON_AddNumberToObject(j,"dwell_ms",c->dwell_ms);
    cJSON_AddNumberToObject(j,"level_deadband",c->level_deadband);
    cJSON_AddNumberToObject(j,"max_level_step",c->max_level_step);
    cJSON *map=cJSON_AddArrayToObject(j,"map");
    for (unsigned i=0;i<c->map_count;++i) {
        cJSON *p=cJSON_CreateObject(); cJSON_AddNumberToObject(p,"temp_f",c->map[i].temp_f);
        cJSON_AddNumberToObject(p,"level",c->map[i].level);
        cJSON_AddNumberToObject(p,"pulse_us",c->map[i].pulse_us); cJSON_AddItemToArray(map,p);
    }
    cJSON_AddBoolToObject(j,"schedule_enabled",c->schedule_enabled);
    cJSON *schedule=cJSON_AddArrayToObject(j,"schedule");
    for(unsigned i=0;i<c->schedule_count;++i) {
        cJSON *p=cJSON_CreateObject();cJSON_AddNumberToObject(p,"days",c->schedule[i].days);
        cJSON_AddNumberToObject(p,"minute",c->schedule[i].minute);
        cJSON_AddNumberToObject(p,"target_f",c->schedule[i].target_f);cJSON_AddItemToArray(schedule,p);
    }
    return j;
}
static void add_sample(cJSON *j,const char *name,th_sample_t p,uint64_t now)
{
    cJSON *o=cJSON_AddObjectToObject(j,name);
    cJSON_AddBoolToObject(o,"fresh",p.valid && now>=p.at_ms && now-p.at_ms<=THERMO_TTL_MS);
    if(p.valid) {cJSON_AddNumberToObject(o,"temp_f",p.f);cJSON_AddNumberToObject(o,"age_ms",now-p.at_ms);}
}
static esp_err_t history_get(httpd_req_t *r)
{
    history_t *snapshot=malloc(sizeof(history));
    if(!snapshot)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");
    xSemaphoreTake(lock,portMAX_DELAY);
    unsigned count=history_count<360?history_count:360;
    unsigned start=history_count>360?history_count-360:0;
    for(unsigned i=0;i<count;++i)snapshot[i]=history[(start+i)%360];
    xSemaphoreGive(lock);
    httpd_resp_set_type(r,"text/csv");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Content-Disposition","attachment; filename=thermostat-history.csv");
    esp_err_t err=httpd_resp_sendstr_chunk(r,"uptime_ms,unix_s,source,temperature_f,target_f,commanded_level\n");
    for(unsigned i=0;i<count && err==ESP_OK;++i) {
        char row[160];history_t p=snapshot[i];
        snprintf(row,sizeof(row),"%llu,%lld,%s,%.3f,%.3f,%.3f\n",(unsigned long long)p.uptime_ms,
            (long long)p.unix_s,th_source_name(p.source),p.temperature_f,p.target_f,p.level);
        err=httpd_resp_sendstr_chunk(r,row);
    }
    free(snapshot);if(err==ESP_OK)err=httpd_resp_send_chunk(r,NULL,0);return err;
}
static esp_err_t get_api(httpd_req_t *r)
{
    if(!strcmp(r->uri,"/api/history"))return history_get(r);
    xSemaphoreTake(lock,portMAX_DELAY);
    cJSON *j=NULL;
    if(!strcmp(r->uri,"/api/config")) j=config_json();
    else if(!strcmp(r->uri,"/api/events")) {
        j=cJSON_CreateArray();
        unsigned begin=event_count>64 ? event_count-64:0;
        for(unsigned i=begin;i<event_count;++i) {
            cJSON *e=cJSON_CreateObject(); cJSON_AddNumberToObject(e,"uptime_ms",events[i%64].ms);
            cJSON_AddStringToObject(e,"message",events[i%64].text); cJSON_AddItemToArray(j,e);
        }
    } else {
        j=cJSON_CreateObject();
        cJSON_AddStringToObject(j,"version","0.5.7");
#ifdef CONFIG_THERMO_SUPERMINI
        cJSON_AddStringToObject(j,"board","HW-747 Super Mini 4 MB");
#else
        cJSON_AddStringToObject(j,"board","ESP32-S3 controller");
#endif
        cJSON_AddNumberToObject(j,"servo_gpio",CONFIG_THERMO_SERVO_GPIO);
        cJSON_AddBoolToObject(j,"simulation",state.simulated);
        cJSON_AddBoolToObject(j,"bench_armed",armed);
        cJSON_AddNumberToObject(j,"arm_remaining_ms",armed && armed_until>now_ms()?armed_until-now_ms():0);
        cJSON_AddBoolToObject(j,"pwm_active",pwm);
        cJSON_AddNumberToObject(j,"last_command_us",last_pulse);
        cJSON_AddBoolToObject(j,"position_synchronized",state.synchronized);
        cJSON_AddBoolToObject(j,"position_measured",false);
        cJSON_AddStringToObject(j,"position_feedback","AS5600 bench telemetry only; dial position unverified");
        controller_encoder_sample_t enc=controller_encoder_snapshot();
        bool fresh=enc.online && now_ms()>=enc.at_ms && now_ms()-enc.at_ms<=2000;
        bool valid=fresh && enc.magnet_detected && !enc.too_weak && !enc.too_strong;
        cJSON *e=cJSON_AddObjectToObject(j,"encoder");
        cJSON_AddBoolToObject(e,"online",fresh);
        cJSON_AddBoolToObject(e,"angle_valid",valid);
        cJSON_AddStringToObject(e,"error",enc.online && !fresh?"Stale reading":esp_err_to_name(enc.error));
        cJSON_AddNumberToObject(e,"sda_gpio",CONFIG_THERMO_ENCODER_SDA);
        cJSON_AddNumberToObject(e,"scl_gpio",CONFIG_THERMO_ENCODER_SCL);
        if(fresh) {
            cJSON_AddBoolToObject(e,"magnet_detected",enc.magnet_detected);
            cJSON_AddBoolToObject(e,"too_weak",enc.too_weak);
            cJSON_AddBoolToObject(e,"too_strong",enc.too_strong);
            cJSON_AddNumberToObject(e,"age_ms",now_ms()-enc.at_ms);
            if(valid) {
                cJSON_AddNumberToObject(e,"raw_angle",enc.raw_angle);
                cJSON_AddNumberToObject(e,"angle_deg",enc.raw_angle*360.0/4096.0);
            }
        }
        cJSON *net=cJSON_AddObjectToObject(j,"network");
        cJSON_AddStringToObject(net,"hostname","CottageThermoController");
        cJSON_AddStringToObject(net,"station_mac",station_mac);
        cJSON_AddStringToObject(net,"ssid",network.ssid);
        cJSON_AddBoolToObject(net,"connected",station_up);
        cJSON_AddBoolToObject(net,"password_configured",*network.password!=0);
        esp_netif_ip_info_t ip={0};char address[16];
        esp_netif_get_ip_info(station_netif,&ip);
        snprintf(address,sizeof(address),IPSTR,IP2STR(&ip.ip));
        cJSON_AddStringToObject(net,"ip",station_up?address:"");
        uint8_t channel=0;wifi_second_chan_t secondary;
        esp_wifi_get_channel(&channel,&secondary);
        cJSON_AddNumberToObject(net,"channel",channel);
        cJSON_AddStringToObject(net,"setup_ssid",CONFIG_THERMO_AP_SSID);
        cJSON_AddBoolToObject(net,"setup_recovery",setup_recovery);
        cJSON_AddNumberToObject(net,"failed_connects",station_failures);
        wifi_sta_list_t ap_clients={0};
        if(esp_wifi_ap_get_sta_list(&ap_clients)==ESP_OK)cJSON_AddNumberToObject(net,"setup_clients",ap_clients.num);
        int8_t tx_power=0;
        if(esp_wifi_get_max_tx_power(&tx_power)==ESP_OK)cJSON_AddNumberToObject(net,"wifi_max_tx_dbm",(double)tx_power/4.0);
        cJSON_AddNumberToObject(net,"http_link_requests",http_link_count);
        if(http_link_count)cJSON_AddNumberToObject(net,"last_http_link_age_ms",now_ms()-last_http_link);
        const cottage_integration_t *profile=controller_integrations_get();
        cJSON_AddBoolToObject(net,"radio_ready",tl_radio_configured());
        cJSON_AddBoolToObject(net,"radio_online",tl_radio_online());
        cJSON_AddStringToObject(net,"radio_peer",profile->peer);
        cJSON_AddStringToObject(j,"reason",state.reason);
        cJSON_AddStringToObject(j,"source",th_source_name(state.source));
        cJSON_AddStringToObject(j,"mode",th_mode_name(state.config.mode));
        cJSON_AddNumberToObject(j,"target_f",state.effective_target_f);
        cJSON_AddBoolToObject(j,"clock_valid",state.clock_valid);
        cJSON_AddBoolToObject(j,"schedule_enabled",state.config.schedule_enabled);
        if(state.source!=TH_NONE) cJSON_AddNumberToObject(j,"temperature_f",state.selected_f);
        if(state.has_command) cJSON_AddNumberToObject(j,"commanded_level",state.commanded_level);
        if(isfinite(state.proposed_level)) cJSON_AddNumberToObject(j,"proposed_level",state.proposed_level);
        cJSON_AddNumberToObject(j,"uptime_ms",now_ms());
        add_sample(j,"room",state.room,now_ms());add_sample(j,"switchbot",state.secondary,now_ms());
    }
    xSemaphoreGive(lock); return json_send(r,j);
}
static bool update_config(cJSON *j)
{
    th_config_t next=state.config;
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,"mode");
    if(v) {
        if(!cJSON_IsString(v)) return false;
        int m;
        for(m=TH_HOLD;m<=TH_AUTO;++m) if(!strcmp(v->valuestring,th_mode_name(m))) break;
        if(m>TH_AUTO) return false;
        next.mode=m;
    }
    struct {const char *key;float *p;} floats[]={
        {"target_f",&next.target_f},{"direct_level",&next.direct_level},
        {"level_deadband",&next.level_deadband},{"max_level_step",&next.max_level_step}};
    for(unsigned i=0;i<sizeof(floats)/sizeof(floats[0]);++i) {
        v=cJSON_GetObjectItemCaseSensitive(j,floats[i].key);
        if(v) {double d;if(!number(j,floats[i].key,&d))return false;*floats[i].p=d;}
    }
    v=cJSON_GetObjectItemCaseSensitive(j,"dwell_ms");
    if(v) {
        double d;if(!number(j,"dwell_ms",&d) || d<10000 || d>3600000 || floor(d)!=d)return false;
        next.dwell_ms=(uint32_t)d;
    }
    v=cJSON_GetObjectItemCaseSensitive(j,"secondary_enabled");
    if(v) {if(!cJSON_IsBool(v))return false;next.secondary_enabled=cJSON_IsTrue(v);}
    bool map_changed=false;
    v=cJSON_GetObjectItemCaseSensitive(j,"map");
    if(v) {
        if(!cJSON_IsArray(v) || cJSON_GetArraySize(v)>THERMO_MAP_MAX)return false;
        next.map_count=cJSON_GetArraySize(v); memset(next.map,0,sizeof(next.map));
        for(unsigned i=0;i<next.map_count;++i) {
            cJSON *p=cJSON_GetArrayItem(v,i);double t,l,u;
            if(!number(p,"temp_f",&t)||!number(p,"level",&l)||!number(p,"pulse_us",&u))return false;
            next.map[i]=(th_point_t){t,l,u};
        }
        map_changed=next.map_count!=state.config.map_count || memcmp(next.map,state.config.map,sizeof(next.map));
    }
    // An explicit target is a persistent manual override of the schedule.
    if(cJSON_HasObjectItem(j,"target_f"))next.schedule_enabled=false;
    v=cJSON_GetObjectItemCaseSensitive(j,"schedule");
    if(v) {
        if(!cJSON_IsArray(v)||cJSON_GetArraySize(v)>THERMO_SCHEDULE_MAX)return false;
        next.schedule_count=cJSON_GetArraySize(v);memset(next.schedule,0,sizeof(next.schedule));
        for(unsigned i=0;i<next.schedule_count;++i) {
            cJSON *p=cJSON_GetArrayItem(v,i);double days,minute,target;
            if(!number(p,"days",&days)||!number(p,"minute",&minute)||!number(p,"target_f",&target)||
               days<1||days>127||floor(days)!=days||minute<0||minute>1439||floor(minute)!=minute)return false;
            next.schedule[i]=(th_schedule_t){.days=(uint8_t)days,.minute=(uint16_t)minute,.target_f=target};
        }
    }
    v=cJSON_GetObjectItemCaseSensitive(j,"schedule_enabled");
    if(v){if(!cJSON_IsBool(v))return false;next.schedule_enabled=cJSON_IsTrue(v);}
    if(!th_config_valid(&next)) return false;
    if(!memcmp(&next,&state.config,sizeof(next))) return true; // Repeated settings are idempotent; avoid NVS wear.
    if(nvs_set_blob(store,CONFIG_KEY,&next,sizeof(next))!=ESP_OK || nvs_commit(store)!=ESP_OK)return false;
    if(map_changed || next.mode==TH_MANUAL) release();
    state.config=next; event("Settings committed"); return true;
}
/* One validator/lock for paired HTTP and ESP-NOW; neither exposes bench PWM. */
static void link_handle(const tl_packet_t *p,tl_packet_t *reply)
{
    xSemaphoreTake(lock,portMAX_DELAY);
    uint64_t now=now_ms();int result=tl_check(&room_guard,p,controller_boot,now);
    if(result==TL_OK&&p->operation!=TL_STATUS){
        room_guard.sequence=p->sequence; /* Failed operations also consume IDs. */
        if(p->operation==TL_TARGET){
            cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"target_f",p->value);
            result=update_config(j)?TL_OK:TL_SAVE_FAILED;cJSON_Delete(j);
        }else if(p->operation==TL_SAMPLE){
            result=th_sample(&state,TH_ROOM,p->value,p->age_ms,now)?TL_OK:TL_INVALID;
        }
    }
    *reply=tl_request(TL_REPLY,p->client,p->sequence);reply->result=result;
    reply->controller_boot=controller_boot;reply->deadline_ms=now;
    reply->target=state.config.schedule_enabled?state.effective_target_f:state.config.target_f;
    reply->flags=TL_HAS_TARGET|(state.simulated?TL_SIMULATION:0)|(controller_mqtt_configured()?TL_MQTT_CONFIGURED:0)|(controller_mqtt_online()?TL_MQTT_ONLINE:0);reply->mode=state.config.mode;
    th_sample_t sample=state.source==TH_ROOM?state.room:state.secondary;
    if(state.source!=TH_NONE&&sample.valid&&now>=sample.at_ms&&now-sample.at_ms<=THERMO_TTL_MS){
        reply->flags|=TL_HAS_TEMP;reply->temperature=sample.f;
    }
    snprintf(reply->reason,sizeof(reply->reason),"%s",state.reason);
    xSemaphoreGive(lock);
}
static esp_err_t link_post(httpd_req_t *r)
{
    cJSON *j=body(r),*v=cJSON_GetObjectItemCaseSensitive(j,"packet");tl_packet_t p,reply;
    bool valid=cJSON_IsString(v)&&tl_unhex(v->valuestring,&p)&&p.operation!=TL_REPLY;cJSON_Delete(j);
    if(!valid)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid room link envelope");
    link_handle(&p,&reply);
    xSemaphoreTake(lock,portMAX_DELAY);++http_link_count;last_http_link=now_ms();xSemaphoreGive(lock);
    char hex[sizeof(reply)*2+1];tl_hex(&reply,hex);
    j=cJSON_CreateObject();cJSON_AddStringToObject(j,"packet",hex);return json_send(r,j);
}
static void integration_restart(void *arg){vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();}
/* Local provisioning takes effect only after restart. */
static esp_err_t network_post(httpd_req_t *r)
{
    cJSON *j=body(r),*ssid=cJSON_GetObjectItemCaseSensitive(j,"ssid"),
        *password=cJSON_GetObjectItemCaseSensitive(j,"password"),*open=cJSON_GetObjectItemCaseSensitive(j,"open");
    network_profile_t next=network;
    bool valid=cJSON_IsString(ssid)&&cJSON_IsString(password)&&cJSON_IsBool(open)&&
        network_profile_patch(&network,ssid->valuestring,password->valuestring,cJSON_IsTrue(open),&next);
    cJSON_Delete(j);
    if(!valid){memset(&next,0,sizeof(next));return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid SSID/password; new secured networks require a password");}
    xSemaphoreTake(lock,portMAX_DELAY);
    bool changed=memcmp(&next,&network,sizeof(next))!=0;
    esp_err_t err=nvs_set_blob(store,"network",&next,sizeof(next));
    if(err==ESP_OK)err=nvs_commit(store);
    if(err==ESP_OK&&changed){release();state.config.mode=TH_HOLD;event("Wi-Fi profile saved; restarting with PWM disabled");}
    xSemaphoreGive(lock);memset(&next,0,sizeof(next));
    if(err!=ESP_OK)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Could not save Wi-Fi profile");
    if(changed&&xTaskCreate(integration_restart,"network_restart",2048,NULL,3,NULL)!=pdPASS)
        return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Saved; restart controller manually");
    return httpd_resp_sendstr(r,changed?"{\"ok\":true,\"restarting\":true}":"{\"ok\":true,\"restarting\":false}");
}
static esp_err_t integrations_post(httpd_req_t *r)
{
    cJSON *j=body(r);bool changed=false;
    xSemaphoreTake(lock,portMAX_DELAY);bool ok=controller_integrations_save(j,&changed);
    if(changed){release();state.config.mode=TH_HOLD;event("Integration profile saved; restart clears bench arm/synchronization");}
    xSemaphoreGive(lock);cJSON_Delete(j);
    if(!ok)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid integration settings or save failed");
    if(changed&&xTaskCreate(integration_restart,"integration_restart",2048,NULL,3,NULL)!=pdPASS)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"Saved but restart could not be scheduled; retry");
    return httpd_resp_sendstr(r,changed?"{\"ok\":true,\"restarting\":true}":"{\"ok\":true,\"restarting\":false}");
}
static esp_err_t post_api(httpd_req_t *r)
{
    cJSON *j=body(r);
    if(!j)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"JSON object required (max 4096 bytes)");
    bool ok=false;const char *error="Invalid request or unmet prerequisites";
    xSemaphoreTake(lock,portMAX_DELAY);
    if(!strcmp(r->uri,"/api/config")) ok=update_config(j);
    else if(!strcmp(r->uri,"/api/temperature")) {
        double f,age;
        th_source_t source=text_is(j,"source","room")?TH_ROOM:text_is(j,"source","switchbot")?TH_SWITCHBOT:TH_NONE;
        if(number(j,"temp_f",&f)&&number(j,"age_ms",&age)&&age>=0&&age<=THERMO_TTL_MS&&floor(age)==age)
            ok=th_sample(&state,source,f,(uint32_t)age,now_ms());
    } else if(!strcmp(r->uri,"/api/clock")) {
        double unix_s;
        if(number(j,"unix_s",&unix_s)&&unix_s>=1767225600&&unix_s<=4102444800&&floor(unix_s)==unix_s) {
            struct timeval tv={.tv_sec=(time_t)unix_s};ok=settimeofday(&tv,NULL)==0;
            if(ok)event("Local clock synchronized by paired client");
        }
    } else if(!strcmp(r->uri,"/api/stop")) {
        release();state.config.mode=TH_HOLD;event("PWM disabled; synchronization cleared; session Hold");ok=true;
    } else if(!strcmp(r->uri,"/api/arm")) {
        if(text_is(j,"confirm","unloaded_bench") && state.config.mode!=TH_MANUAL) {
            armed=true;armed_until=now_ms()+900000;event("Bench output armed for 15 minutes");ok=true;
        }
    } else if(!strcmp(r->uri,"/api/sync")) {
        double l;
        if((state.simulated || (armed && now_ms()<armed_until)) && number(j,"level",&l) &&
           text_is(j,"confirm",state.simulated?"simulation":"observed_bench_position")) {
            ok=th_sync(&state,l);
            if(ok) event(state.simulated?"Simulation synchronized":"Manual bench synchronization; NOT encoder verified");
        }
    } else if(!strcmp(r->uri,"/api/pulse")) {
        double u;
        if(armed && now_ms()<armed_until && state.config.mode==TH_HOLD && number(j,"us",&u) && floor(u)==u && u>=500 && u<=2500) {
            ok=pulse((int)u)==ESP_OK;
            th_release(&state);
            if(ok)event("Explicit raw bench PWM command; thermostat synchronization cleared");
        }
    }
    xSemaphoreGive(lock);cJSON_Delete(j);
    if(!ok)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,error);
    return httpd_resp_sendstr(r,"{\"ok\":true}");
}
static void control_task(void *unused)
{
    unsigned minute=0;
    for(;;) {
        xSemaphoreTake(lock,portMAX_DELAY);
        uint64_t now=now_ms();
        if(armed && now>=armed_until) {release();event("Bench arm expired; PWM disabled");}
        time_t wall=time(NULL);struct tm local={0};localtime_r(&wall,&local);
        th_clock(&state,wall>=1767225600,(local.tm_wday+6)%7,local.tm_hour*60+local.tm_min);
        th_action_t a=th_tick(&state,now);
        if(a.move) {
            if(state.simulated) th_commit_move(&state,a,now);
            else if(armed && now<armed_until) {
                if(pulse(a.pulse_us)==ESP_OK) th_commit_move(&state,a,now);
                else {release();state.reason="pwm_error";event("PWM error; motion disabled");}
            } else state.reason="bench_arm_required";
            if(state.last_move_ms==now) {
                char msg[96];snprintf(msg,sizeof(msg),"%s level %.2f / %d us (command, not measured)",
                    state.simulated?"SIMULATED":"BENCH",a.level,a.pulse_us);event(msg);
            }
        }
        if(strcmp(last_reason,state.reason)) {
            snprintf(last_reason,sizeof(last_reason),"%s",state.reason);event(state.reason);
        }
        if(++minute>=60) {
            minute=0;
            history[history_count++%360]=(history_t){.uptime_ms=now,.unix_s=state.clock_valid?wall:0,
                .temperature_f=state.selected_f,.target_f=state.effective_target_f,
                .level=state.has_command?state.commanded_level:NAN,.source=state.source};
            ESP_LOGI(TAG,"Sample source=%s temp=%.2f target=%.2f mode=%s reason=%s",
                th_source_name(state.source),state.selected_f,state.config.target_f,
                th_mode_name(state.config.mode),state.reason);
        }
        xSemaphoreGive(lock);vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
/* Setup AP and ESP-NOW share one radio. A stable setup session temporarily
 * has priority over station scans and ESP-NOW channel rendezvous. */
static void enter_setup_recovery(const char *reason)
{
    if(atomic_exchange(&setup_recovery,true))return;
    station_connecting=false;
    ESP_LOGW(TAG,"%s: pausing LAN retries and ESP-NOW channel search. Join %s at http://192.168.4.1",
        reason,CONFIG_THERMO_AP_SSID);
    esp_err_t err=esp_wifi_disconnect();
    if(err!=ESP_OK)ESP_LOGW(TAG,"Recovery Wi-Fi disconnect returned %s",esp_err_to_name(err));
}
static void wifi_event(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    if(base==WIFI_EVENT&&id==WIFI_EVENT_AP_START)
        ESP_LOGI(TAG,"Setup access point started: SSID=%s channel=6, no captive DNS; open http://192.168.4.1",CONFIG_THERMO_AP_SSID);
    if(base==WIFI_EVENT&&id==WIFI_EVENT_AP_STACONNECTED)
        ESP_LOGI(TAG,"Setup client joined; station retries and ESP-NOW channel scanning pause");
    if(base==WIFI_EVENT&&id==WIFI_EVENT_AP_STADISCONNECTED)
        ESP_LOGI(TAG,"Setup client left");
    if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){
        wifi_event_sta_disconnected_t *e=data;
        station_up=false;station_connecting=false;
        if(setup_recovery)return;
        unsigned failures=atomic_fetch_add(&station_failures,1)+1;
        ESP_LOGW(TAG,"LAN Wi-Fi disconnected (reason %u, failure %u); ESP-NOW configured=%s",
            e->reason,failures,tl_radio_configured()?"yes":"no");
        if(controller_wifi_recovery_needed(failures,tl_radio_configured()))
            enter_setup_recovery("Three failed LAN connections without an ESP-NOW peer");
        else reconnect_at=(uint32_t)now_ms()+(tl_radio_configured()?20000:2000);
    }
    if(base==IP_EVENT&&id==IP_EVENT_STA_LOST_IP){
        station_up=false;
        if(!setup_recovery){station_connecting=true;connect_at=(uint32_t)now_ms();}
    }
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) {
        station_up=true;station_connecting=false;station_failures=0;setup_recovery=false;
        ip_event_got_ip_t *e=data;ESP_LOGI(TAG,"Controller LAN address: " IPSTR,IP2STR(&e->ip_info.ip));
    }
}
static void reconnect_task(void *arg)
{
#ifdef CONFIG_THERMO_SUPERMINI
    uint32_t boot_held_from=0;
#endif
    for(;;){
        uint32_t now=(uint32_t)now_ms();
#ifdef CONFIG_THERMO_SUPERMINI
        /* Hold BOOT for 3 s AFTER startup; holding it while applying power
         * would enter the ROM download bootloader instead. */
        if(gpio_get_level(GPIO_NUM_0)==0){
            if(!boot_held_from)boot_held_from=now?now:1;
            if(!setup_recovery&&(uint32_t)(now-boot_held_from)>=3000U)
                enter_setup_recovery("BOOT held for three seconds after startup");
        }else boot_held_from=0;
#endif
        if(setup_recovery){vTaskDelay(pdMS_TO_TICKS(250));continue;}
        wifi_sta_list_t clients={0};
        bool setup_client=esp_wifi_ap_get_sta_list(&clients)==ESP_OK&&clients.num>0;
        if(!station_up&&!setup_client){
            /* Keep the setup AP on one channel during initial discovery. */
            if(!station_connecting&&(int32_t)(now-setup_discovery_until)>=0)tl_radio_search_tick();
            controller_wifi_retry_t retry={.connecting=station_connecting,.started=connect_at,.due=reconnect_at};
            controller_wifi_action_t action=controller_wifi_retry_tick(&retry,now,tl_radio_configured());
            if(action!=CONTROLLER_WIFI_WAIT&&!station_up){
                station_connecting=retry.connecting;connect_at=retry.started;reconnect_at=retry.due;
                if(action==CONTROLLER_WIFI_ABORT){
                    unsigned failures=atomic_fetch_add(&station_failures,1)+1;
                    if(controller_wifi_recovery_needed(failures,tl_radio_configured()))
                        enter_setup_recovery("Repeated LAN authentication timeouts without an ESP-NOW peer");
                    else {
                        esp_err_t err=esp_wifi_disconnect();
                        ESP_LOGW(TAG,"Controller Wi-Fi attempt timed out; retry scheduled (disconnect=%d)",(int)err);
                    }
                }else if(*network.ssid){
                    ESP_LOGI(TAG,"Controller Wi-Fi reconnect attempt");
                    if(esp_wifi_connect()!=ESP_OK){station_connecting=false;reconnect_at=now+(tl_radio_configured()?20000:2000);}
                }else station_connecting=false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
static void network_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    bool station=strlen(network.ssid)>0;
    station_netif=esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(station_netif,"CottageThermoController"));
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL));
    wifi_config_t ap={0};snprintf((char *)ap.ap.ssid,sizeof(ap.ap.ssid),"%s",CONFIG_THERMO_AP_SSID);
    ap.ap.ssid_len=strlen((char *)ap.ap.ssid);ap.ap.channel=6;ap.ap.max_connection=4;ap.ap.authmode=WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
    if(station) {
        wifi_config_t sta={0};memcpy(sta.sta.ssid,network.ssid,sizeof(sta.sta.ssid));
        memcpy(sta.sta.password,network.password,sizeof(sta.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&sta));
    }
    ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
#ifdef CONFIG_THERMO_SUPERMINI
    /* Bench A/B test: cap shared AP/STA/ESP-NOW radio at 13 dBm.
     * Lower TX power may help stability but reduces maximum range. */
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(52));
    int8_t tx_power=0;ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&tx_power));
    ESP_LOGI(TAG,"Super Mini Wi-Fi max TX power: %.2f dBm",tx_power/4.0);
#endif
    uint8_t mac[6];ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA,mac));
    snprintf(station_mac,sizeof(station_mac),"%02X:%02X:%02X:%02X:%02X:%02X",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    ESP_LOGI(TAG,"Controller station MAC: %s; hostname: CottageThermoController",station_mac);
    const cottage_integration_t *integration=controller_integrations_get();
    tl_radio_start_config(link_handle,integration->radio_enabled,integration->peer,integration->key);
    /* Give the bench AP 15 seconds without station scans after startup. */
    station_connecting=false;
    reconnect_at=(uint32_t)now_ms()+15000U;
    setup_discovery_until=reconnect_at;
    ESP_LOGI(TAG,"Setup AP discovery window 15 s; station SSID configured=%s",station?"yes":"no");
    ESP_ERROR_CHECK(xTaskCreate(reconnect_task,"wifi_retry",3072,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    httpd_config_t c=HTTPD_DEFAULT_CONFIG();c.max_uri_handlers=18;c.stack_size=8192;
    httpd_handle_t h;ESP_ERROR_CHECK(httpd_start(&h,&c));
    httpd_uri_t root={.uri="/",.method=HTTP_GET,.handler=root_get};ESP_ERROR_CHECK(httpd_register_uri_handler(h,&root));
    const char *gets[]={"/api/status","/api/config","/api/events","/api/history"};
    for(unsigned i=0;i<4;++i) {httpd_uri_t u={.uri=gets[i],.method=HTTP_GET,.handler=get_api};ESP_ERROR_CHECK(httpd_register_uri_handler(h,&u));}
    httpd_uri_t link={.uri="/api/link",.method=HTTP_POST,.handler=link_post};ESP_ERROR_CHECK(httpd_register_uri_handler(h,&link));
    httpd_uri_t net={.uri="/api/network",.method=HTTP_POST,.handler=network_post};ESP_ERROR_CHECK(httpd_register_uri_handler(h,&net));
    httpd_uri_t integrations={.uri="/api/integrations",.method=HTTP_POST,.handler=integrations_post};ESP_ERROR_CHECK(httpd_register_uri_handler(h,&integrations));
    const char *posts[]={"/api/config","/api/temperature","/api/stop","/api/arm","/api/sync","/api/pulse","/api/clock"};
    for(unsigned i=0;i<7;++i) {httpd_uri_t u={.uri=posts[i],.method=HTTP_POST,.handler=post_api};ESP_ERROR_CHECK(httpd_register_uri_handler(h,&u));}
    ESP_LOGI(TAG,"Open bench Wi-Fi %s; http://192.168.4.1",CONFIG_THERMO_AP_SSID);
}
void app_main(void)
{
    gpio_config_t pin={.pin_bit_mask=1ULL<<CONFIG_THERMO_SERVO_GPIO,.mode=GPIO_MODE_OUTPUT};
    ESP_ERROR_CHECK(gpio_config(&pin));ESP_ERROR_CHECK(gpio_set_level(CONFIG_THERMO_SERVO_GPIO,0));
    // Do not erase NVS on version/full errors: retain calibration for deliberate recovery.
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(nvs_open("thermostat",NVS_READWRITE,&store));
    th_config_t c=th_defaults();size_t size=sizeof(c);
    th_config_t saved;
    if(nvs_get_blob(store,CONFIG_KEY,&saved,&size)==ESP_OK && size==sizeof(saved) && th_config_valid(&saved))c=saved;
    bool simulated=true;
#ifdef CONFIG_THERMO_LIVE_BENCH
    simulated=false;
#endif
    th_init(&state,&c,simulated);state.selected_f=NAN;state.proposed_level=NAN;
    lock=xSemaphoreCreateMutex();assert(lock);
    event("Boot: PWM disabled; position unknown; no restoration or homing");
    ESP_LOGW(TAG,"Attended bench firmware. Automatic output: %s",simulated?"SIMULATION":"LIVE BENCH");
    setenv("TZ","EST5EDT,M3.2.0/2,M11.1.0/2",1);tzset();
    controller_boot=((uint64_t)esp_random()<<32)|esp_random();if(!controller_boot)controller_boot=1;
    network.magic=NETWORK_PROFILE_MAGIC;
    snprintf(network.ssid,sizeof(network.ssid),"%s",CONFIG_THERMO_WIFI_SSID);
    snprintf(network.password,sizeof(network.password),"%s",CONFIG_THERMO_WIFI_PASSWORD);
    network_profile_t saved_network;size=sizeof(saved_network);
    if(nvs_get_blob(store,"network",&saved_network,&size)==ESP_OK&&size==sizeof(saved_network)&&network_profile_valid(&saved_network))network=saved_network;
    memset(&saved_network,0,sizeof(saved_network));
    controller_integrations_init();
#ifdef CONFIG_THERMO_SUPERMINI
    gpio_config_t recovery_button={.pin_bit_mask=1ULL<<GPIO_NUM_0,.mode=GPIO_MODE_INPUT,.pull_up_en=GPIO_PULLUP_ENABLE};
    ESP_ERROR_CHECK(gpio_config(&recovery_button));
#endif
    network_start();
    controller_mqtt_start();
    controller_encoder_start();
#ifdef CONFIG_THERMO_SUPERMINI
    ESP_LOGI(TAG,"HW-747 Super Mini bench: 4 MB flash; PSRAM disabled; servo GPIO%d; encoder SDA%d/SCL%d",
        CONFIG_THERMO_SERVO_GPIO,CONFIG_THERMO_ENCODER_SDA,CONFIG_THERMO_ENCODER_SCL);
#endif
    if(strlen(network.ssid)) {
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();
    }
    xTaskCreate(control_task,"thermostat",4096,NULL,5,NULL);
}

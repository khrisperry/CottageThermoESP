/* Optional HA climate discovery. Broker credentials are independent of the HA
 * long-lived REST token; neither credential is published or logged here. */
#include "controller_mqtt.h"
#include "climate_discovery.h"
#include "sdkconfig.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <time.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_crt_bundle.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#if __has_include("../secrets.h")
#include "../secrets.h"
#endif
#ifndef COTTAGE_MQTT_URL
#define COTTAGE_MQTT_URL ""
#endif
#ifndef COTTAGE_MQTT_USERNAME
#define COTTAGE_MQTT_USERNAME ""
#endif
#ifndef COTTAGE_MQTT_PASSWORD
#define COTTAGE_MQTT_PASSWORD ""
#endif
static esp_mqtt_client_handle_t mqtt;
static QueueHandle_t commands;
static atomic_bool connected,announce,subscribe_pending;
static atomic_uint generation;
static char identity[48],base[80],availability[100];
typedef struct {char key[8],value[40];int64_t at;unsigned generation;} mqtt_command_t;
static bool topic_is(esp_mqtt_event_handle_t e,const char *suffix)
{
    char t[120];int n=snprintf(t,sizeof(t),"%s/%s",base,suffix);
    return n==e->topic_len&&e->topic&&!memcmp(e->topic,t,n);
}
static void mqtt_event(void *arg,esp_event_base_t event_base,int32_t id,void *data)
{
    esp_mqtt_event_handle_t e=data;
    if(id==MQTT_EVENT_CONNECTED){++generation;connected=true;announce=true;subscribe_pending=true;}
    else if(id==MQTT_EVENT_DISCONNECTED)connected=false;
    else if(id==MQTT_EVENT_DATA){
        if(e->topic_len==20&&e->topic&&!memcmp(e->topic,"homeassistant/status",20)){
            if(e->data_len==6&&!memcmp(e->data,"online",6))announce=true;
            return;
        }
        /* Never apply retained commands, MQTT retransmissions or fragments. */
        if(e->retain||e->dup||e->current_data_offset||e->total_data_len!=e->data_len||e->data_len<=0||e->data_len>=40||memchr(e->data,0,e->data_len))return;
        mqtt_command_t c={.at=esp_timer_get_time(),.generation=generation};
        const char *key=topic_is(e,"target/set")?"target":topic_is(e,"mode/set")?"mode":topic_is(e,"preset/set")?"preset":NULL;
        if(!key)return;
        snprintf(c.key,sizeof(c.key),"%s",key);memcpy(c.value,e->data,e->data_len);
        xQueueSend(commands,&c,0);
    }
}
static bool publish(const char *suffix,const char *body,bool retain)
{
    char topic[120];snprintf(topic,sizeof(topic),"%s/%s",base,suffix);
    /* QoS 0 never accumulates stale state in an offline outbox. */
    return connected&&esp_mqtt_client_publish(mqtt,topic,body,0,0,retain)>=0;
}
static bool discovery(void)
{
    cJSON *j=climate_discovery(identity,base);char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!s)return false;
    char topic[128];snprintf(topic,sizeof(topic),"homeassistant/climate/%s/config",identity);
    bool ok=esp_mqtt_client_publish(mqtt,topic,s,0,1,1)>=0;free(s);
    return ok;
}
static bool subscribe_topics(void)
{
    bool ok=true;char topic[128];
    const char *suffix[]={"target/set","mode/set","preset/set"};
    for(unsigned n=0;n<3;++n){snprintf(topic,sizeof(topic),"%s/%s",base,suffix[n]);if(esp_mqtt_client_subscribe(mqtt,topic,0)<0)ok=false;}
    if(esp_mqtt_client_subscribe(mqtt,"homeassistant/status",0)<0)ok=false;
    return ok;
}
static void cycle(void)
{
    mqtt_command_t c;
    while(xQueueReceive(commands,&c,0)==pdTRUE){
        if(!connected||c.generation!=generation||esp_timer_get_time()-c.at>5000000)continue;
        char *end=NULL;float f=strtof(c.value,&end);
        if(!strcmp(c.key,"target")&&(!end||*end||end==c.value||!isfinite(f)))continue;
        if(!controller_climate_command(c.key,f,c.value))ESP_LOGW("thermo_mqtt","Climate command rejected by controller");
    }
    if(connected){
        if(atomic_exchange(&subscribe_pending,false)&&!subscribe_topics())subscribe_pending=true;
        if(atomic_exchange(&announce,false)&&!discovery())announce=true;
        cJSON *j=controller_climate_state();char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
        if(s){if(publish("state",s,true))publish("availability","online",true);free(s);}
    }
}
static void worker(void *arg)
{
    for(;;){cycle();vTaskDelay(pdMS_TO_TICKS(1000));}
}
static void start_task(void *arg)
{
    /* TLS requires a trusted clock. Plain local mqtt:// is an explicit LAN
     * choice; mqtts:// always validates a certificate. */
    bool tls=!strncmp(COTTAGE_MQTT_URL,"mqtts://",8);
    while(!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")||
          !esp_netif_is_netif_up(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"))||
          (tls&&time(NULL)<1767225600))vTaskDelay(pdMS_TO_TICKS(1000));
    esp_mqtt_client_config_t cfg={
        .broker.address.uri=COTTAGE_MQTT_URL,
        .broker.verification.crt_bundle_attach=tls?esp_crt_bundle_attach:NULL,
        .credentials.client_id=identity,.credentials.username=COTTAGE_MQTT_USERNAME,
        .credentials.authentication.password=COTTAGE_MQTT_PASSWORD,
        .session.keepalive=20,.session.last_will={.topic=availability,.msg="offline",.qos=1,.retain=1},
        .buffer.size=2048,.task.stack_size=6144,.network.reconnect_timeout_ms=5000};
    mqtt=esp_mqtt_client_init(&cfg);
    if(!mqtt){ESP_LOGE("thermo_mqtt","Could not initialize MQTT");vTaskDelete(NULL);return;}
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt,MQTT_EVENT_ANY,mqtt_event,NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt));worker(NULL);
}
void controller_mqtt_start(void)
{
    if(!*COTTAGE_MQTT_URL){ESP_LOGI("thermo_mqtt","Climate discovery not configured; set controller/secrets.h");return;}
    if((strncmp(COTTAGE_MQTT_URL,"mqtt://",7)&&strncmp(COTTAGE_MQTT_URL,"mqtts://",8))||strchr(COTTAGE_MQTT_URL,'@')){
        ESP_LOGE("thermo_mqtt","Invalid broker origin; use separate credentials");return;
    }
    uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
    snprintf(identity,sizeof(identity),"cottage_thermostat_%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    snprintf(base,sizeof(base),"cottage/thermostat/%s",identity);snprintf(availability,sizeof(availability),"%s/availability",base);
    commands=xQueueCreate(8,sizeof(mqtt_command_t));ESP_ERROR_CHECK(commands?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(start_task,"climate_mqtt",6144,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}

/* Optional wall-panel telemetry. No controller command API is referenced here. */
#include "room_mqtt.h"
#include "room_mqtt_discovery.h"
#include "room_settings.h"
#include "room_display.h"
#include "wifi_setup.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const cottage_integration_t *profile;
static esp_mqtt_client_handle_t mqtt;
static atomic_bool connected,announce,subscribe_pending;
static atomic_int stage,broker_error,socket_error; /* 0 disabled,1 WiFi,2 clock,3 connecting,4 online,5 error */
static char identity[64],base[96],availability[112],birth[96];
static const char *description(void)
{
    if(!profile||!profile->mqtt_enabled)return "Not configured";
    switch(stage){case 1:return "Waiting for Wi-Fi";case 2:return "Waiting for valid time";case 3:return "Connecting / reconnecting";case 4:return "Connected (monitoring only)";case 5:return broker_error?"Broker rejected connection; check credentials":"Broker/TLS connection failed";default:return "Starting";}
}
int room_mqtt_state(void){return !profile||!profile->mqtt_enabled?0:connected?2:1;}
bool room_mqtt_reannounce(void){if(!connected)return false;announce=true;return true;}
cJSON *room_mqtt_status(void)
{
    cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"connected",connected);cJSON_AddStringToObject(j,"status",description());
    cJSON_AddStringToObject(j,"client_id",identity);cJSON_AddBoolToObject(j,"read_only",true);
    cJSON_AddNumberToObject(j,"broker_return_code",broker_error);cJSON_AddNumberToObject(j,"socket_errno",socket_error);return j;
}
static void mqtt_event(void *arg,esp_event_base_t event_base,int32_t id,void *data)
{
    esp_mqtt_event_handle_t e=data;
    if(id==MQTT_EVENT_BEFORE_CONNECT){
        /* Certificate verification must still allow idle-task time slicing. */
        vTaskPrioritySet(NULL,0);if(stage!=5)stage=3;
        ESP_LOGI("room_mqtt","Connecting to broker %s as %s",profile->broker,identity);
    }else if(id==MQTT_EVENT_CONNECTED){connected=true;stage=4;broker_error=0;socket_error=0;announce=true;subscribe_pending=true;ESP_LOGI("room_mqtt","Broker connected; Home Assistant monitoring only");}
    else if(id==MQTT_EVENT_DISCONNECTED){connected=false;if(stage!=5)stage=3;ESP_LOGW("room_mqtt","Broker disconnected; standalone thermostat continues");}
    else if(id==MQTT_EVENT_ERROR){connected=false;stage=5;
        if(e&&e->error_handle){broker_error=e->error_handle->error_type==MQTT_ERROR_TYPE_CONNECTION_REFUSED?e->error_handle->connect_return_code:0;socket_error=e->error_handle->esp_transport_sock_errno;}
        ESP_LOGW("room_mqtt","Connection failed: broker code=%d socket errno=%d",(int)broker_error,(int)socket_error);
    }else if(id==MQTT_EVENT_DATA&&e&&e->topic&&e->data&&e->topic_len==(int)strlen(birth)&&!memcmp(e->topic,birth,e->topic_len)&&
             !e->current_data_offset&&e->total_data_len==e->data_len&&e->data_len==6&&!memcmp(e->data,"online",6))announce=true;
    /* All other incoming traffic is ignored. There is no command subscription. */
}
static bool discovery(void)
{
    const char *keys[]={"temperature","target","controller"};bool ok=true;
    for(unsigned i=0;i<3;++i){cJSON *j=room_mqtt_discovery(identity,base,profile->name,esp_app_get_description()->version,i);char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
        if(!s){ok=false;continue;}char topic[224];snprintf(topic,sizeof(topic),"%s/%s/%s/%s/config",profile->prefix,i==2?"binary_sensor":"sensor",identity,keys[i]);
        if(esp_mqtt_client_publish(mqtt,topic,s,0,1,1)<0)ok=false;
        free(s);
    }return ok;
}
static void cycle(void)
{
    if(!connected)return;
    /* Subscribe before discovery; a synchronous retained birth cannot loop. */
    if(atomic_exchange(&subscribe_pending,false)&&esp_mqtt_client_subscribe(mqtt,birth,0)<0)subscribe_pending=true;
    if(atomic_exchange(&announce,false)&&!discovery())announce=true;
    cJSON *j=room_display_mqtt_state();char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);if(!s)return;
    char topic[112];snprintf(topic,sizeof(topic),"%s/state",base);
    if(esp_mqtt_client_publish(mqtt,topic,s,0,0,1)>=0)esp_mqtt_client_publish(mqtt,availability,"online",0,0,1);
    free(s);
}
static bool connect_client(void)
{
    bool tls=!strncmp(profile->broker,"mqtts://",8);
    esp_mqtt_client_config_t cfg={.broker.address.uri=profile->broker,.broker.verification.crt_bundle_attach=tls?esp_crt_bundle_attach:NULL,
        .credentials.client_id=identity,.credentials.username=profile->username,.credentials.authentication.password=profile->password,
        .session.keepalive=20,.session.last_will={.topic=availability,.msg="offline",.qos=1,.retain=1},
        .buffer.size=2048,.task.stack_size=8192,.task.priority=1,.network.reconnect_timeout_ms=5000};
    mqtt=esp_mqtt_client_init(&cfg);if(!mqtt)return false;
    if(esp_mqtt_client_register_event(mqtt,MQTT_EVENT_ANY,mqtt_event,NULL)==ESP_OK&&esp_mqtt_client_start(mqtt)==ESP_OK)return true;
    esp_mqtt_client_destroy(mqtt);mqtt=NULL;return false;
}
static void worker(void *arg)
{
    bool tls=!strncmp(profile->broker,"mqtts://",8);
    while(!room_wifi_connected()||(tls&&time(NULL)<1767225600)){stage=room_wifi_connected()?2:1;vTaskDelay(pdMS_TO_TICKS(1000));}
    stage=3;
    while(!connect_client()){stage=5;ESP_LOGE("room_mqtt","Could not start broker client; retrying");vTaskDelay(pdMS_TO_TICKS(5000));}
    for(;;){cycle();vTaskDelay(pdMS_TO_TICKS(2000));}
}
void room_mqtt_start(void)
{
    profile=room_settings_integrations();if(!profile->mqtt_enabled||!*profile->broker){ESP_LOGI("room_mqtt","Monitoring not configured; set MQTT on the local webpage");return;}
    uint8_t mac[6];ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_STA));
    snprintf(identity,sizeof(identity),"cottage_room_%02x%02x%02x%02x%02x%02x",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    snprintf(base,sizeof(base),"cottage/thermostat/%s",identity);snprintf(availability,sizeof(availability),"%s/availability",base);
    snprintf(birth,sizeof(birth),"homeassistant/status");stage=1;
    ESP_ERROR_CHECK(xTaskCreate(worker,"room_mqtt",8192,NULL,1,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
}

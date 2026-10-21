/* Real wall MQTT publisher. The fake broker has no actuator command API. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/room_mqtt.c"
#include "../../controller/main/controller_mqtt.c"
static cottage_integration_t fixture={.mqtt_enabled=true,.prefix="homeassistant",.name="Cottage thermostat",.broker="mqtt://192.0.2.30:1883",.username="user",.password="fixture-password"};
static unsigned subscriptions,discoveries,states;static bool controller_up=false,sensor_up=true;static char last_state[512];
const cottage_integration_t *room_settings_integrations(void){return &fixture;}
bool room_wifi_connected(void){return true;}
int esp_read_mac(uint8_t *m,int type){memset(m,2,6);return ESP_OK;}
void vTaskPrioritySet(void *task,unsigned priority){assert(priority==0);}
int xTaskCreate(void(*fn)(void*),const char *name,int stack,void *a,int priority,void *out){assert(stack==8192);return pdPASS;}
void vTaskDelay(int n){}
void vTaskDelete(void *task){}
static esp_app_desc_t app={.version="0.12.0"};
const esp_app_desc_t *esp_app_get_description(void){return &app;}
int esp_crt_bundle_attach(void *p){return ESP_OK;}
int esp_mqtt_client_destroy(void *p){return ESP_OK;}
void *esp_mqtt_client_init(const esp_mqtt_client_config_t *cfg){
 assert(!strcmp(cfg->broker.address.uri,fixture.broker));assert(!strcmp(cfg->credentials.username,"user"));assert(!strcmp(cfg->credentials.authentication.password,"fixture-password"));
 assert(cfg->session.last_will.retain&&!strcmp(cfg->session.last_will.msg,"offline"));return (void*)1;
}
int esp_mqtt_client_register_event(void *c,int id,void(*cb)(void*,esp_event_base_t,int32_t,void*),void *a){return ESP_OK;}
int esp_mqtt_client_start(void *c){return ESP_OK;}
cJSON *room_display_mqtt_state(void){cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"controller_online",controller_up);cJSON_AddBoolToObject(j,"sensor_fresh",sensor_up);if(sensor_up)cJSON_AddNumberToObject(j,"temperature_f",67);else cJSON_AddNullToObject(j,"temperature_f");cJSON_AddNullToObject(j,"target_f");return j;}
int esp_mqtt_client_publish(void *c,const char *topic,const char *data,int n,int qos,int retain){
 assert(!strstr(topic,"/set"));
 if(strstr(topic,"/config")){++discoveries;assert(qos==1&&retain);assert(!strstr(data,"command_topic")&&!strstr(data,"password")&&!strstr(data,"fixture-password"));assert(!strstr(topic,"/climate/"));}
 if(strstr(topic,"/state")){++states;snprintf(last_state,sizeof(last_state),"%s",data);}
 return 1;
}
int esp_mqtt_client_subscribe(void *c,const char *topic,int qos){
 assert(!strcmp(topic,"homeassistant/status"));++subscriptions;
 esp_mqtt_event_t e={.topic="homeassistant/status",.topic_len=20,.data="online",.data_len=6,.total_data_len=6,.retain=true};mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);return 1;
}
int main(void){
 controller_mqtt_start();assert(!controller_mqtt_configured()&&!controller_mqtt_online());
 room_mqtt_start();assert(room_mqtt_state()==1);assert(connect_client()); /* No controller check gates broker startup. */
 mqtt_event(NULL,NULL,MQTT_EVENT_BEFORE_CONNECT,NULL);mqtt_event(NULL,NULL,MQTT_EVENT_CONNECTED,NULL);cycle();assert(room_mqtt_state()==2&&subscriptions==1&&discoveries==3&&states==1);
 assert(strstr(last_state,"\"controller_online\":false")&&strstr(last_state,"67"));
 for(unsigned n=0;n<10;++n)cycle();
 assert(discoveries==3&&subscriptions==1);
 char topic[128];snprintf(topic,sizeof(topic),"%s/target/set",base);esp_mqtt_event_t command={.topic=topic,.topic_len=(int)strlen(topic),.data="80",.data_len=2,.total_data_len=2};
 mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&command);cycle();assert(discoveries==3&&!controller_up); /* Broker commands have no effect. */
 assert(room_mqtt_reannounce());cycle();assert(discoveries==6);
 sensor_up=false;cycle();assert(strstr(last_state,"\"temperature_f\":null"));
 esp_mqtt_error_codes_t error={.error_type=MQTT_ERROR_TYPE_CONNECTION_REFUSED,.connect_return_code=5};esp_mqtt_event_t e={.error_handle=&error};mqtt_event(NULL,NULL,MQTT_EVENT_ERROR,&e);assert(room_mqtt_state()==1&&strstr(description(),"credentials"));
 mqtt_event(NULL,NULL,MQTT_EVENT_DISCONNECTED,NULL);assert(!room_mqtt_reannounce());unsigned count=states;cycle();assert(states==count);
 mqtt_event(NULL,NULL,MQTT_EVENT_CONNECTED,NULL);cycle();assert(subscriptions==2&&discoveries==9);
 puts("PASS: wall MQTT starts without controller, read-only discovery, only HA birth subscription, ignored command injection, stale sensor clearing, auth diagnostics and reconnect");
}

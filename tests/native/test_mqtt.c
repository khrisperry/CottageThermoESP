/* Exercise actual MQTT event/worker code with synchronous fake broker birth. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../controller/main/controller_mqtt.c"
static int64_t clock_us=1000000;
static int subscriptions,discoveries,applied;
static mqtt_command_t queued[8];static unsigned head,tail;
int64_t esp_timer_get_time(void){return clock_us;}
int xQueueSend(void *q,const void *p,int wait){if(tail-head==8)return 0;queued[tail++%8]=*(const mqtt_command_t*)p;return 1;}
int xQueueReceive(void *q,void *p,int wait){if(head==tail)return 0;*(mqtt_command_t*)p=queued[head++%8];return 1;}
int esp_mqtt_client_publish(void *client,const char *topic,const char *data,int len,int qos,int retain){if(strstr(topic,"/config")){++discoveries;assert(retain&&qos==1);}return 1;}
int esp_mqtt_client_subscribe(void *client,const char *topic,int qos){
 ++subscriptions;assert(qos==0);
 if(!strcmp(topic,"homeassistant/status")){esp_mqtt_event_t e={.topic="homeassistant/status",.topic_len=20,.data="online",.data_len=6,.total_data_len=6,.retain=true};mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);}
 return 1;
}
bool controller_climate_command(const char *key,float target,const char *value){assert(!strcmp(key,"target"));if(target<45||target>80)return false;++applied;return true;}
cJSON *controller_climate_state(void){cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"fresh",true);return j;}
static esp_mqtt_event_t command(const char *payload){static char topic[120];snprintf(topic,sizeof(topic),"%s/target/set",base);return (esp_mqtt_event_t){.topic=topic,.topic_len=(int)strlen(topic),.data=(char*)payload,.data_len=(int)strlen(payload),.total_data_len=(int)strlen(payload)};}
int main(void){
 mqtt=(void*)1;commands=(void*)1;snprintf(identity,sizeof(identity),"test");snprintf(base,sizeof(base),"cottage/thermostat/test");
 mqtt_event(NULL,NULL,MQTT_EVENT_CONNECTED,NULL);cycle();assert(discoveries==1&&subscriptions==4);
 for(int n=0;n<8;++n){cycle();}assert(discoveries==1&&subscriptions==4); /* retained birth cannot loop */
 esp_mqtt_event_t e=command("67");e.retain=true;mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);cycle();assert(!applied);
 e=command("67");e.dup=true;mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);cycle();assert(!applied);
 e=command("67");e.total_data_len=4;mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);cycle();assert(!applied);
 e=command("67");mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);cycle();assert(applied==1);
 e=command("nan");mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);cycle();assert(applied==1);
 e=command("81");mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);cycle();assert(applied==1);
 e=command("68");mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);clock_us+=6000000;cycle();assert(applied==1);
 e=command("69");mqtt_event(NULL,NULL,MQTT_EVENT_DATA,&e);mqtt_event(NULL,NULL,MQTT_EVENT_DISCONNECTED,NULL);mqtt_event(NULL,NULL,MQTT_EVENT_CONNECTED,NULL);cycle();assert(applied==1&&discoveries==2&&subscriptions==8);
 puts("PASS: MQTT retained birth, rediscovery, command validation/expiry and session recovery");
}

#pragma once
#include "host.h"
typedef const char *esp_event_base_t;
typedef void *esp_mqtt_client_handle_t;
enum {MQTT_EVENT_ANY=-1,MQTT_EVENT_CONNECTED=1,MQTT_EVENT_DISCONNECTED,MQTT_EVENT_DATA};
typedef struct {char *topic,*data;int topic_len,data_len,total_data_len,current_data_offset;bool retain,dup;} esp_mqtt_event_t;
typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;
typedef struct {
 struct {struct {const char *uri;} address;struct {int (*crt_bundle_attach)(void*);} verification;} broker;
 struct {const char *client_id,*username;struct {const char *password;} authentication;} credentials;
 struct {int keepalive;struct {const char *topic,*msg;int qos,retain;} last_will;} session;
 struct {int size;} buffer;
 struct {int stack_size;} task;
 struct {int reconnect_timeout_ms;} network;
} esp_mqtt_client_config_t;
int esp_mqtt_client_publish(void *client,const char *topic,const char *data,int len,int qos,int retain);
int esp_mqtt_client_subscribe(void *client,const char *topic,int qos);
void *esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
int esp_mqtt_client_register_event(void *client,int id,void(*cb)(void*,esp_event_base_t,int32_t,void*),void *arg);
int esp_mqtt_client_start(void *client);

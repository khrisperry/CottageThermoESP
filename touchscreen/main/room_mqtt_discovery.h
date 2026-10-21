#pragma once
#include <stdio.h>
#include "cJSON.h"
/* Monitoring only: no climate/number/switch entities or command topics. */
static inline cJSON *room_mqtt_discovery(const char *id,const char *base,const char *name,const char *version,unsigned entity)
{
    const char *keys[]={"temperature","target","controller"};
    const char *names[]={"Room temperature","Target temperature","Controller connection"};
    if(entity>=3)return NULL;
    char unique[96];snprintf(unique,sizeof(unique),"%s_%s",id,keys[entity]);
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"name",names[entity]);cJSON_AddStringToObject(j,"unique_id",unique);
    cJSON_AddStringToObject(j,"~",base);cJSON_AddStringToObject(j,"state_topic","~/state");
    cJSON_AddStringToObject(j,"json_attributes_topic","~/state");cJSON_AddNumberToObject(j,"expire_after",45);
    cJSON_AddStringToObject(j,"device_class",entity==2?"connectivity":"temperature");
    if(entity==2){cJSON_AddStringToObject(j,"value_template","{{ 'online' if value_json.controller_online else 'offline' }}");cJSON_AddStringToObject(j,"payload_on","online");cJSON_AddStringToObject(j,"payload_off","offline");}
    else{cJSON_AddStringToObject(j,"unit_of_measurement","°F");cJSON_AddStringToObject(j,"state_class","measurement");
        cJSON_AddStringToObject(j,"value_template",entity==0?"{{ value_json.temperature_f if value_json.temperature_f is not none else 'None' }}":"{{ value_json.target_f if value_json.target_f is not none else 'None' }}");}
    cJSON *a=cJSON_AddArrayToObject(j,"availability"),*online=cJSON_CreateObject();cJSON_AddStringToObject(online,"topic","~/availability");cJSON_AddItemToArray(a,online);
    if(entity!=2){cJSON *fresh=cJSON_CreateObject();cJSON_AddStringToObject(fresh,"topic","~/state");
        cJSON_AddStringToObject(fresh,"value_template",entity==0?"{{ 'online' if value_json.sensor_fresh else 'offline' }}":"{{ 'online' if value_json.controller_online and value_json.target_f is not none else 'offline' }}");cJSON_AddItemToArray(a,fresh);}
    cJSON_AddStringToObject(j,"availability_mode","all");
    cJSON *d=cJSON_AddObjectToObject(j,"device"),*ids=cJSON_AddArrayToObject(d,"identifiers");cJSON_AddItemToArray(ids,cJSON_CreateString(id));
    cJSON_AddStringToObject(d,"name",name);cJSON_AddStringToObject(d,"manufacturer","CottageThermoESP");cJSON_AddStringToObject(d,"model","Waveshare 4B standalone thermostat");cJSON_AddStringToObject(d,"sw_version",version);
    return j;
}

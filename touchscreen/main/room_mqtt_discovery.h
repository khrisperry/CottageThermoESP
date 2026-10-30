#pragma once
#include <stdio.h>
#include <stdbool.h>
#include "cJSON.h"
enum { ROOM_MQTT_ENTITY_COUNT=13 };
static const char *const room_mqtt_entity_keys[]={"temperature","target","controller","thermostat",
    "wifi_status","ha_status","controller_status","espnow_status","sensor_status","ships_status","mqtt_status","connection","away_hold"};
static const char *const room_mqtt_service_keys[]={"wifi","ha","controller","espnow","sensor","ships","mqtt"};
static inline const char *room_mqtt_component(unsigned entity){return entity==3?"climate":entity==12?"switch":entity==2||entity==11?"binary_sensor":"sensor";}
/* Climate target requests terminate at the wall thermostat, never at the servo. */
static inline cJSON *room_mqtt_discovery(const char *id,const char *base,const char *name,const char *version,unsigned entity)
{
    const char *names[]={"Room temperature","Target temperature","Controller connection","Thermostat",
        "Wi-Fi status","Home Assistant status","Heat controller status","ESP-NOW status","Local sensor status","Ship tracking status","MQTT status","Thermostat connection","Away / Freeze Hold (50 F)"};
    if(entity>=ROOM_MQTT_ENTITY_COUNT)return NULL;
    char unique[96];snprintf(unique,sizeof(unique),"%s_%s",id,room_mqtt_entity_keys[entity]);
    cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"name",names[entity]);cJSON_AddStringToObject(j,"unique_id",unique);
    cJSON_AddStringToObject(j,"~",base);cJSON_AddStringToObject(j,"json_attributes_topic","~/state");
    if(entity==3){
        cJSON_AddStringToObject(j,"temperature_command_topic","~/target/set");
        cJSON_AddStringToObject(j,"temperature_state_topic","~/state");
        cJSON_AddStringToObject(j,"temperature_state_template","{{ value_json.target_f if value_json.target_f is not none else 'None' }}");
        cJSON_AddStringToObject(j,"current_temperature_topic","~/state");
        cJSON_AddStringToObject(j,"current_temperature_template","{{ value_json.temperature_f if value_json.temperature_f is not none else 'None' }}");
        /* HA Climate preset (not HVAC off): Away keeps heat available at
         * 50 F. HA never directly arms or controls the furnace servo. */
        cJSON_AddStringToObject(j,"preset_mode_command_topic","~/preset/set");
        cJSON_AddStringToObject(j,"preset_mode_state_topic","~/state");
        cJSON_AddStringToObject(j,"preset_mode_value_template",
            "{{ 'away' if value_json.away else 'home' }}");
        cJSON *presets=cJSON_AddArrayToObject(j,"preset_modes");
        /* Explicit Home and Away presets. Home Assistant itself adds
         * the built-in None entry, which cannot be renamed by MQTT.
         * The state reports Home instead of None when away is disabled. */
        cJSON_AddItemToArray(presets,cJSON_CreateString("home"));
        cJSON_AddItemToArray(presets,cJSON_CreateString("away"));
        cJSON_AddStringToObject(j,"mode_state_topic","~/state");
        cJSON_AddStringToObject(j,"mode_state_template","{{ 'heat' if value_json.mode == 'thermostat' else 'None' }}");
        cJSON *m=cJSON_AddArrayToObject(j,"modes");cJSON_AddItemToArray(m,cJSON_CreateString("heat"));
        cJSON_AddStringToObject(j,"temperature_unit","F");cJSON_AddNumberToObject(j,"min_temp",45);cJSON_AddNumberToObject(j,"max_temp",80);
        cJSON_AddNumberToObject(j,"temp_step",0.5);cJSON_AddNumberToObject(j,"precision",0.1);cJSON_AddBoolToObject(j,"optimistic",false);cJSON_AddBoolToObject(j,"retain",false);
    }else if(entity==12){
        /* This explicit switch appears on the HA thermostat device, even
         * when the climate card frontend does not show its preset picker.
         * It uses the same authenticated thermostat->controller command. */
        cJSON_AddStringToObject(j,"command_topic","~/away/set");
        cJSON_AddStringToObject(j,"state_topic","~/state");
        cJSON_AddStringToObject(j,"value_template","{{ 'ON' if value_json.away else 'OFF' }}");
        cJSON_AddStringToObject(j,"payload_on","ON");
        cJSON_AddStringToObject(j,"payload_off","OFF");
        cJSON_AddBoolToObject(j,"optimistic",false);
        cJSON_AddBoolToObject(j,"retain",false);
    }else if(entity>=4){
        cJSON_AddStringToObject(j,"entity_category","diagnostic");
        if(entity==11){
            cJSON_AddStringToObject(j,"state_topic","~/availability");cJSON_AddStringToObject(j,"device_class","connectivity");
            cJSON_AddStringToObject(j,"payload_on","online");cJSON_AddStringToObject(j,"payload_off","offline");
        }else{
            char value[128];snprintf(value,sizeof(value),"{{ value_json.services.%s | default('Unknown') }}",room_mqtt_service_keys[entity-4]);
            cJSON_AddStringToObject(j,"state_topic","~/state");cJSON_AddStringToObject(j,"value_template",value);cJSON_AddNumberToObject(j,"expire_after",45);
        }
    }else{
        cJSON_AddStringToObject(j,"state_topic","~/state");cJSON_AddNumberToObject(j,"expire_after",45);
        cJSON_AddStringToObject(j,"device_class",entity==2?"connectivity":"temperature");
        if(entity==2){cJSON_AddStringToObject(j,"value_template","{{ 'online' if value_json.controller_online else 'offline' }}");cJSON_AddStringToObject(j,"payload_on","online");cJSON_AddStringToObject(j,"payload_off","offline");}
        else{cJSON_AddStringToObject(j,"unit_of_measurement","°F");cJSON_AddStringToObject(j,"state_class","measurement");
            cJSON_AddStringToObject(j,"value_template",entity==0?"{{ value_json.temperature_f if value_json.temperature_f is not none else 'None' }}":"{{ value_json.target_f if value_json.target_f is not none else 'None' }}");}
    }
    if(entity!=11){
    cJSON *a=cJSON_AddArrayToObject(j,"availability"),*online=cJSON_CreateObject();cJSON_AddStringToObject(online,"topic","~/availability");cJSON_AddItemToArray(a,online);
    if((entity<4&&entity!=2)||entity==12){cJSON *fresh=cJSON_CreateObject();cJSON_AddStringToObject(fresh,"topic","~/state");
        cJSON_AddStringToObject(fresh,"value_template",
            entity==0?"{{ 'online' if value_json.sensor_fresh else 'offline' }}":
            entity==3?"{{ 'online' if value_json.controller_online and value_json.mode == 'thermostat' and value_json.target_f is not none else 'offline' }}":
            entity==12?"{{ 'online' if value_json.controller_online else 'offline' }}":
            "{{ 'online' if value_json.controller_online and value_json.target_f is not none else 'offline' }}");
        cJSON_AddItemToArray(a,fresh);}
    cJSON_AddStringToObject(j,"availability_mode","all");
    }
    cJSON *d=cJSON_AddObjectToObject(j,"device"),*ids=cJSON_AddArrayToObject(d,"identifiers");cJSON_AddItemToArray(ids,cJSON_CreateString(id));
    cJSON_AddStringToObject(d,"name",name);cJSON_AddStringToObject(d,"manufacturer","CottageThermoESP");cJSON_AddStringToObject(d,"model","Waveshare 4B standalone thermostat");cJSON_AddStringToObject(d,"sw_version",version);
    return j;
}

#pragma once
#include <stdbool.h>
#include "cJSON.h"
static inline cJSON *climate_discovery_named(const char *id,const char *base,const char *name)
{
    cJSON *j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"name",name);cJSON_AddStringToObject(j,"unique_id",id);
    cJSON_AddStringToObject(j,"~",base);
    cJSON_AddStringToObject(j,"temperature_unit","F");cJSON_AddNumberToObject(j,"min_temp",45);
    cJSON_AddNumberToObject(j,"max_temp",80);cJSON_AddNumberToObject(j,"temp_step",0.5);cJSON_AddNumberToObject(j,"precision",0.1);
    cJSON_AddBoolToObject(j,"optimistic",false);
    cJSON_AddStringToObject(j,"temperature_command_topic","~/target/set");
    cJSON_AddStringToObject(j,"temperature_state_topic","~/state");
    cJSON_AddStringToObject(j,"temperature_state_template","{{ value_json.target }}");
    cJSON_AddStringToObject(j,"current_temperature_topic","~/state");
    cJSON_AddStringToObject(j,"current_temperature_template","{{ value_json.temperature if value_json.temperature is not none else 'None' }}");
    cJSON_AddStringToObject(j,"mode_command_topic","~/mode/set");
    cJSON_AddStringToObject(j,"mode_state_topic","~/state");
    cJSON_AddStringToObject(j,"mode_state_template","{{ value_json.mode }}");
    cJSON *m=cJSON_AddArrayToObject(j,"modes");cJSON_AddItemToArray(m,cJSON_CreateString("heat"));
    /* The mechanical knob cannot promise OFF. Hold only stops adaptation. */
    cJSON_AddStringToObject(j,"preset_mode_command_topic","~/preset/set");
    cJSON_AddStringToObject(j,"preset_mode_state_topic","~/state");
    cJSON_AddStringToObject(j,"preset_mode_value_template","{{ value_json.preset }}");
    cJSON *p=cJSON_AddArrayToObject(j,"preset_modes");cJSON_AddItemToArray(p,cJSON_CreateString("hold"));cJSON_AddItemToArray(p,cJSON_CreateString("thermostat"));
    cJSON_AddStringToObject(j,"json_attributes_topic","~/state");
    cJSON_AddStringToObject(j,"availability_mode","all");
    cJSON *a=cJSON_AddArrayToObject(j,"availability"),*broker=cJSON_CreateObject(),*sensor=cJSON_CreateObject();
    cJSON_AddStringToObject(broker,"topic","~/availability");cJSON_AddItemToArray(a,broker);
    cJSON_AddStringToObject(sensor,"topic","~/state");
    cJSON_AddStringToObject(sensor,"value_template","{{ 'online' if value_json.fresh else 'offline' }}");cJSON_AddItemToArray(a,sensor);
    cJSON *d=cJSON_AddObjectToObject(j,"device"),*ids=cJSON_AddArrayToObject(d,"identifiers");
    cJSON_AddItemToArray(ids,cJSON_CreateString(id));cJSON_AddStringToObject(d,"name",name);
    cJSON_AddStringToObject(d,"manufacturer","CottageThermoESP");
    cJSON_AddStringToObject(d,"model","Standalone thermostat controller");cJSON_AddStringToObject(d,"sw_version","0.4.0");
    return j;
}

static inline cJSON *climate_discovery(const char *id,const char *base){return climate_discovery_named(id,base,"Cottage thermostat");}

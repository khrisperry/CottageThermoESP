#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../controller/main/climate_discovery.h"
static const char *text(cJSON *j,const char *key){cJSON *v=cJSON_GetObjectItem(j,key);assert(cJSON_IsString(v));return v->valuestring;}
int main(void)
{
    cJSON *j=climate_discovery("cottage_thermostat_123456789abc","cottage/thermostat/123456789abc");
    assert(!strcmp(text(j,"unique_id"),"cottage_thermostat_123456789abc"));
    assert(!strcmp(text(j,"temperature_unit"),"F"));
    assert(!strcmp(text(j,"temperature_command_topic"),"~/target/set"));
    assert(cJSON_GetObjectItem(j,"min_temp")->valuedouble==45&&cJSON_GetObjectItem(j,"max_temp")->valuedouble==80);
    cJSON *m=cJSON_GetObjectItem(j,"modes");assert(cJSON_GetArraySize(m)==1&&!strcmp(cJSON_GetArrayItem(m,0)->valuestring,"heat"));
    assert(!cJSON_HasObjectItem(j,"action_topic")); /* no invented burner telemetry */
    assert(cJSON_IsFalse(cJSON_GetObjectItem(j,"optimistic")));
    assert(!strcmp(text(j,"availability_mode"),"all"));assert(cJSON_GetArraySize(cJSON_GetObjectItem(j,"availability"))==2);
    assert(strstr(text(j,"current_temperature_template"),"None"));
    char *a=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    j=climate_discovery("cottage_thermostat_123456789abc","cottage/thermostat/123456789abc");char *b=cJSON_PrintUnformatted(j);
    assert(!strcmp(a,b));assert(!strstr(a,"password")&&!strstr(a,"token")&&!strstr(a,"pulse"));
    cJSON_free(a);cJSON_free(b);cJSON_Delete(j);
    puts("PASS: stable HA climate discovery, Fahrenheit bounds, confirmed state, availability and supported modes");
}

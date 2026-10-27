/* Home Assistant MQTT Climate must expose actual Away preset, with no
 * command topics capable of arming or directly controlling the servo. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "room_mqtt_discovery.h"
static const char *value(cJSON *j,const char *key){
    cJSON *x=cJSON_GetObjectItemCaseSensitive(j,key);
    return cJSON_IsString(x)?x->valuestring:"";
}
int main(void)
{
    cJSON *j=room_mqtt_discovery("room_f4","cottage/thermostat/room_f4","Cottage Room","0.14.0",3);
    assert(j&&strcmp(value(j,"preset_mode_command_topic"),"~/preset/set")==0);
    assert(!strcmp(value(j,"preset_mode_state_topic"),"~/state"));
    assert(strstr(value(j,"preset_mode_value_template"),"value_json.away"));
    assert(!strcmp(value(j,"temperature_command_topic"),"~/target/set"));
    cJSON *m=cJSON_GetObjectItemCaseSensitive(j,"modes");
    assert(cJSON_GetArraySize(m)==1&&!strcmp(cJSON_GetArrayItem(m,0)->valuestring,"heat"));
    cJSON *presets=cJSON_GetObjectItemCaseSensitive(j,"preset_modes");
    assert(cJSON_GetArraySize(presets)==2);
    assert(!strcmp(cJSON_GetArrayItem(presets,0)->valuestring,"none"));
    assert(!strcmp(cJSON_GetArrayItem(presets,1)->valuestring,"away"));
    assert(!cJSON_GetObjectItemCaseSensitive(j,"mode_command_topic"));
    assert(!cJSON_GetObjectItemCaseSensitive(j,"fan_mode_command_topic"));
    assert(!cJSON_GetObjectItemCaseSensitive(j,"swing_mode_command_topic"));
    cJSON_Delete(j);
    j=room_mqtt_discovery("room_f4","cottage/thermostat/room_f4","Cottage Room","0.14.0",0);
    assert(j&&!cJSON_GetObjectItemCaseSensitive(j,"preset_mode_command_topic"));
    cJSON_Delete(j);
    puts("PASS: HA heat-only climate Away/None preset, target-only commands, no servo/HVAC power controls");
    return 0;
}

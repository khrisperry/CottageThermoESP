#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/room_mqtt_discovery.h"
int main(void){
 char *first[ROOM_MQTT_ENTITY_COUNT]={0};
 for(unsigned i=0;i<ROOM_MQTT_ENTITY_COUNT;++i){cJSON *j=room_mqtt_discovery("panel_test","cottage/panel_test","Test thermostat","0.12.1",i);assert(j);
  cJSON *a=cJSON_GetObjectItem(j,"availability");assert(cJSON_GetArraySize(a)==(i==11?0:i==2||i>=4?1:2));
  cJSON *device=cJSON_GetObjectItem(j,"device");assert(!strcmp(cJSON_GetArrayItem(cJSON_GetObjectItem(device,"identifiers"),0)->valuestring,"panel_test"));
  first[i]=cJSON_PrintUnformatted(j);assert(!strstr(first[i],"mode_command_topic")&&!strstr(first[i],"power_command_topic")&&!strstr(first[i],"password")&&!strstr(first[i],"token"));
  if(i==3){assert(!strcmp(cJSON_GetObjectItem(j,"temperature_command_topic")->valuestring,"~/target/set"));assert(!cJSON_IsTrue(cJSON_GetObjectItem(j,"optimistic")));assert(!cJSON_HasObjectItem(j,"expire_after"));assert(cJSON_GetObjectItem(j,"min_temp")->valuedouble==45&&cJSON_GetObjectItem(j,"max_temp")->valuedouble==80);}
  else assert(!strstr(first[i],"command_topic"));
  if(i>=4){assert(!strcmp(cJSON_GetObjectItem(j,"entity_category")->valuestring,"diagnostic"));
   if(i==11){assert(!a&&!strcmp(cJSON_GetObjectItem(j,"state_topic")->valuestring,"~/availability"));}
   else {assert(cJSON_GetObjectItem(j,"expire_after")->valueint==45);assert(strstr(cJSON_GetObjectItem(j,"value_template")->valuestring,room_mqtt_service_keys[i-4]));assert(cJSON_GetArraySize(a)==1);}}
  for(unsigned k=0;k<i;++k){cJSON *previous=cJSON_Parse(first[k]);assert(strcmp(cJSON_GetObjectItem(previous,"unique_id")->valuestring,cJSON_GetObjectItem(j,"unique_id")->valuestring));cJSON_Delete(previous);}
  cJSON_Delete(j);
  j=room_mqtt_discovery("panel_test","cottage/panel_test","Test thermostat","0.12.1",i);char *again=cJSON_PrintUnformatted(j);assert(!strcmp(first[i],again));cJSON_free(again);cJSON_Delete(j);
 }
 assert(!room_mqtt_discovery("id","base","name","version",ROOM_MQTT_ENTITY_COUNT));
 assert(strcmp(first[0],first[1])&&strcmp(first[0],first[2]));for(unsigned i=0;i<ROOM_MQTT_ENTITY_COUNT;++i)cJSON_free(first[i]);
 puts("PASS: stable Climate and eleven monitoring entities on one device, target-only control, independent availability, no raw servo/mode commands");
}

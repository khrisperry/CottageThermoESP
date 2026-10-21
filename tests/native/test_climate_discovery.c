#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/room_mqtt_discovery.h"
int main(void){
 char *first[3]={0};
 for(unsigned i=0;i<3;++i){cJSON *j=room_mqtt_discovery("panel_test","cottage/panel_test","Test thermostat","0.12.0",i);assert(j);
  cJSON *a=cJSON_GetObjectItem(j,"availability");assert(cJSON_GetArraySize(a)==(i==2?1:2));
  cJSON *device=cJSON_GetObjectItem(j,"device");assert(!strcmp(cJSON_GetArrayItem(cJSON_GetObjectItem(device,"identifiers"),0)->valuestring,"panel_test"));
  first[i]=cJSON_PrintUnformatted(j);assert(!strstr(first[i],"command_topic")&&!strstr(first[i],"password")&&!strstr(first[i],"token"));cJSON_Delete(j);
  j=room_mqtt_discovery("panel_test","cottage/panel_test","Test thermostat","0.12.0",i);char *again=cJSON_PrintUnformatted(j);assert(!strcmp(first[i],again));cJSON_free(again);cJSON_Delete(j);
 }
 assert(strcmp(first[0],first[1])&&strcmp(first[0],first[2]));for(unsigned i=0;i<3;++i)cJSON_free(first[i]);
 puts("PASS: three stable read-only entities on one thermostat device, independent sensor/controller availability, no command topics or secrets");
}

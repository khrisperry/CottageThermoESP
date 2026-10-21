/* Controller persistence path, including ambiguous-save retries and secrets. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../controller/main/controller_integrations.c"
static cottage_integration_t persisted;
static size_t stored_size;
static unsigned writes;
int nvs_open(const char *name,int mode,nvs_handle_t *h){*h=1;return 0;}
void nvs_close(nvs_handle_t h){}
int nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){if(!stored_size||*size<stored_size)return -1;memcpy(out,&persisted,stored_size);*size=stored_size;return 0;}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size){assert(size==sizeof(persisted));memcpy(&persisted,data,size);stored_size=size;++writes;return 0;}
int nvs_commit(nvs_handle_t h){return 0;}
static cJSON *profile(void){return cJSON_Parse("{\"mqtt\":{\"enabled\":true,\"broker\":\"mqtts://broker.example:8883\",\"username\":\"thermostat\",\"password\":\"test-password\",\"prefix\":\"homeassistant\",\"name\":\"Cottage\"}}");}
int main(void){
 memset(&current,0,sizeof(current));cJSON *j=profile();bool changed=false;
 assert(!controller_integrations_save(j,&changed)&&writes==0); /* Controller cannot enable MQTT. */
 cottage_integration_t panel={0};cJSON *m=cJSON_GetObjectItem(j,"mqtt");assert(cottage_integration_patch(&panel,m,false));assert(!strcmp(panel.password,"test-password"));
 cJSON_DeleteItemFromObject(m,"password");assert(cottage_integration_patch(&panel,m,false)&&!strcmp(panel.password,"test-password"));
 cJSON_ReplaceItemInObject(m,"broker",cJSON_CreateString("mqtt://host:bad"));assert(!cottage_integration_patch(&panel,m,false));
 cJSON_ReplaceItemInObject(m,"broker",cJSON_CreateString("mqtt://host:70000"));assert(!cottage_integration_patch(&panel,m,false));
 cJSON_ReplaceItemInObject(m,"broker",cJSON_CreateString("mqtt://host:1883"));cJSON_AddBoolToObject(m,"clear_password",true);assert(cottage_integration_patch(&panel,m,false)&&!*panel.password);
 cJSON_DeleteItemFromObject(j,"mqtt");cJSON *radio=cJSON_AddObjectToObject(j,"radio");cJSON_AddBoolToObject(radio,"enabled",true);cJSON_AddStringToObject(radio,"peer","FF:FF:FF:FF:FF:FF");cJSON_AddStringToObject(radio,"key","0123456789abcdef0123456789abcdef");assert(!controller_integrations_save(j,&changed)&&writes==0);
 cJSON_ReplaceItemInObject(radio,"peer",cJSON_CreateString("02:00:00:00:00:01"));assert(controller_integrations_save(j,&changed)&&changed&&writes==1);
 assert(controller_integrations_save(j,&changed)&&changed&&writes==1); /* Ambiguous-save retry. */
 current=persisted;assert(controller_integrations_save(j,&changed)&&!changed&&writes==1);
 cJSON_Delete(j);puts("PASS: controller rejects MQTT enable, radio config persists/idempotently retries, wall broker credentials retain/clear and validate");
}

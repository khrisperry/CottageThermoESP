/* Exercise production direct HA normalization/requests with a fake TLS transport. */
#define ROOM_HA_TEST 1
#ifdef ROOM_HA_EMPTY
#define COTTAGE_HA_TOKEN ""
#else
#define COTTAGE_HA_TOKEN "test-long-lived-token"
#endif
#include "host.h"
#include <stdio.h>
#include <string.h>
static int64_t fake_time=1000000;
static int gets,posts,frames,fail_code,missing=-1;
static bool uncertain,unsupported,bad_temp;
static char sent_path[160],sent_body[256];
#include "../../touchscreen/main/room_ha.c"
int64_t esp_timer_get_time(void){return fake_time;}
int xSemaphoreTake(void *p,int n){return 1;}
int xSemaphoreGive(void *p){return 1;}
void *xSemaphoreCreateMutex(void){return (void*)1;}
int xTaskCreate(void(*f)(void*),const char*n,int a,void*b,int c,void*d){return 1;}
void vTaskDelay(int n){}
void *heap_caps_malloc(size_t n,int cap){return malloc(n);}
esp_err_t room_http(const char *origin,const char *header,const char *token,const char *path,const char *post,void *out,size_t cap,int timeout,int *status,int *length)
{
 assert(!strcmp(origin,"https://cottageha.greenledges.net:8123"));assert(!strcmp(header,"Authorization"));assert(!strcmp(token,"Bearer test-long-lived-token"));
 *length=0;*status=fail_code?fail_code:200;if(fail_code)return 0;
 if(post){++posts;snprintf(sent_path,sizeof(sent_path),"%s",path);snprintf(sent_body,sizeof(sent_body),"%s",post);if(uncertain){*status=0;return ESP_FAIL;}*length=snprintf(out,cap,"[]");return 0;}
 if(strstr(path,"/api/camera_proxy/")){++frames;assert(strstr(path,"?width=480&height=320"));memcpy(out,"\xff\xd8\xff\xd9",4);*length=4;return 0;}
 ++gets;assert(strstr(path,"/api/states/")==path);
 int key=-1;for(int k=0;k<HA_COUNT;++k)if(!strcmp(path+12,ha_specs[k].entity))key=k;
 assert(key>=0);if(key==missing){*status=404;return 0;}
 *length=snprintf(out,cap,"{\"entity_id\":\"%s\",\"state\":\"%s\",\"attributes\":{\"supported_features\":%d,\"unit_of_measurement\":\"°F\",\"access_token\":\"never-return-this\",\"current_position\":55}}",ha_specs[key].entity,key==8?(bad_temp?"nan":"65.2"):key==3?"Running":"off",unsupported?0:8203);
 return 0;
}
static cJSON *item(cJSON *root,int k){return cJSON_GetArrayItem(cJSON_GetObjectItem(root,"items"),k);}
static void command(int key,const char *action,const char *id,bool success)
{
 char json[256];snprintf(json,sizeof(json),"{\"key\":\"%s\",\"action\":\"%s\",\"request_id\":\"%s\"}",ha_specs[key].key,action,id);
 cJSON *p=cJSON_Parse(json);int code;cJSON *result=room_ha_command(p,&code);assert(code==200);assert(cJSON_IsTrue(cJSON_GetObjectItem(result,"accepted"))==success);cJSON_Delete(p);cJSON_Delete(result);
}
int main(void)
{
#ifdef ROOM_HA_EMPTY
 room_ha_start();ha_poll_once();assert(gets==0&&posts==0&&frames==0);cJSON *empty=room_ha_status();assert(cJSON_IsFalse(cJSON_GetObjectItem(empty,"configured")));cJSON_Delete(empty);puts("PASS: missing local HA token sends no requests");return 0;
#endif
 room_ha_start();ha_poll_once();assert(gets==11&&posts==0&&frames==0);
 cJSON *root=room_ha_status();assert(cJSON_GetArraySize(cJSON_GetObjectItem(root,"items"))==11);assert(cJSON_IsTrue(cJSON_GetObjectItem(root,"connected")));
 assert(!strcmp(ha_string(item(root,3),"state"),"Running"));assert(cJSON_GetObjectItem(item(root,8),"value")->valuedouble==65.2);
 char *json=cJSON_PrintUnformatted(root);assert(!strstr(json,"never-return-this"));assert(!strstr(json,COTTAGE_HA_TOKEN));free(json);cJSON_Delete(root);
 command(7,"on","request-01",true);assert(!strcmp(sent_path,"/api/services/input_boolean/turn_on"));assert(strstr(sent_body,"input_boolean.kids_heat"));assert(!strstr(sent_body,"climate"));int before=posts;command(7,"on","request-01",true);assert(posts==before);
 command(0,"off","request-02",true);assert(!strcmp(sent_path,"/api/services/light/turn_off"));command(2,"stop","request-03",true);assert(!strcmp(sent_path,"/api/services/cover/stop_cover"));command(9,"start","request-04",true);assert(!strcmp(sent_path,"/api/services/vacuum/start"));
 uncertain=true;command(9,"stop","request-05",false);before=posts;uncertain=false;ha_poll_once();command(9,"stop","request-05",false);assert(posts==before);
 missing=3;ha_poll_once();root=room_ha_status();assert(cJSON_IsFalse(cJSON_GetObjectItem(item(root,3),"available")));assert(cJSON_IsTrue(cJSON_GetObjectItem(item(root,0),"available")));cJSON_Delete(root);missing=-1;
 unsupported=true;bad_temp=true;ha_poll_once();root=room_ha_status();assert(cJSON_GetArraySize(cJSON_GetObjectItem(item(root,2),"actions"))==0);assert(cJSON_IsFalse(cJSON_GetObjectItem(item(root,8),"available")));cJSON_Delete(root);unsupported=false;bad_temp=false;
 ha_poll_once();char frame[10];int code,n;assert(room_ha_camera(frame,sizeof(frame),&code,&n)==0&&code==200&&n==4);assert(frames==1);
 fake_time+=31000000;root=room_ha_status();assert(cJSON_IsFalse(cJSON_GetObjectItem(root,"connected")));cJSON_Delete(root);
 cJSON *p=cJSON_Parse("{\"key\":\"furnace\",\"action\":\"on\",\"request_id\":\"invalid-01\"}");root=room_ha_command(p,&code);assert(code==400);cJSON_Delete(root);cJSON_Delete(p);
 fail_code=401;ha_poll_once();assert(ha_auth_failed);before=gets;fail_code=0;ha_poll_once();assert(gets==before);root=room_ha_status();assert(cJSON_IsFalse(cJSON_GetObjectItem(root,"connected")));assert(strstr(ha_string(root,"error"),"token rejected"));cJSON_Delete(root);
 puts("PASS: direct HA bearer requests, eleven mappings, services, filtering, deduplication, unknown outcomes, stale/missing/features, camera and auth backoff");
}

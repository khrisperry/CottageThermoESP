#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../touchscreen/main/room_ships.h"
static size_t allocated,peak;
typedef union {size_t size;max_align_t align;} allocation_t;
static void *allocate(size_t n){allocation_t *p=malloc(sizeof(*p)+n);if(!p)return NULL;p->size=n;allocated+=n;if(allocated>peak)peak=allocated;return p+1;}
static void release(void *p){if(p){allocation_t *a=(allocation_t*)p-1;allocated-=a->size;free(a);}}
static bool decode(cJSON *j,room_ships_t *out){char *s=cJSON_PrintUnformatted(j);assert(s);bool ok=room_ships_decode(s,strlen(s),out);cJSON_free(s);return ok;}
int main(int argc,char **argv){
 assert(argc==2);FILE *file=fopen(argv[1],"rb");assert(file);char body[ROOM_SHIP_BODY_MAX+1];size_t n=fread(body,1,sizeof(body)-1,file);body[n]=0;fclose(file);
 cJSON_Hooks hooks={allocate,release};cJSON_InitHooks(&hooks);room_ships_t result;
 assert(room_ships_decode(body,n,&result));assert(result.count==12&&result.feed&&result.service&&result.ships[0].speed_valid);
 for(unsigned i=0;i<2000;++i){assert(room_ships_decode(body,n,&result));assert(allocated==0);}assert(peak<100000);
 cJSON *j=cJSON_Parse(body);assert(j);cJSON_ReplaceItemInObject(j,"schemaVersion",cJSON_CreateNumber(2));assert(!decode(j,&result));cJSON_ReplaceItemInObject(j,"schemaVersion",cJSON_CreateNumber(1));
 cJSON *list=cJSON_GetObjectItem(j,"vessels"),*ship=cJSON_GetArrayItem(list,0);cJSON_ReplaceItemInObject(ship,"latitude",cJSON_CreateNumber(91));assert(!decode(j,&result));cJSON_ReplaceItemInObject(ship,"latitude",cJSON_CreateNumber(44.3));
 cJSON_AddItemToArray(list,cJSON_Duplicate(ship,true));assert(!decode(j,&result));cJSON_DeleteItemFromArray(list,12);
 cJSON_ReplaceItemInObject(ship,"name",cJSON_CreateString("A\nNAME WITH ÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉÉ"));assert(decode(j,&result));assert(!strchr(result.ships[0].name,'\n'));
 cJSON_ReplaceItemInObject(j,"vessels",cJSON_CreateArray());cJSON_ReplaceItemInObject(j,"nearbyCount",cJSON_CreateNumber(0));assert(decode(j,&result)&&result.count==0&&result.service); /* empty is healthy */
 cJSON_Delete(j);assert(!allocated);assert(!room_ships_decode(body,ROOM_SHIP_BODY_MAX+1,&result));assert(!room_ships_decode("[[[[[[[[[[[[[[[[[0]]]]]]]]]]]]]]]]]",35,&result));
 printf("PASS: service 1.4.3 contract, empty/invalid/oversized/stale inputs, UTF-8 bounds, 2000 refreshes with zero retained allocations; parser peak %zu bytes\n",peak);
}

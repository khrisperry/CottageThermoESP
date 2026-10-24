/* Real settings/OTA handlers; fake NVS, HTTP stream and inactive flash slot. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define reply settings_reply
#include "../../touchscreen/main/room_settings.c"
#undef reply
#include "../../touchscreen/main/room_ota.c"
static uint8_t stored_profile[8192];static size_t stored_size;static char stored_code[32];
static int writes,selects,aborts,begins,confirms;static bool bad_end,bad_write,bad_begin,no_slot,bad_select;
static esp_partition_t running={"factory",4194304},inactive={"ota_0",4194304};
static esp_app_desc_t description={.version="test"};
const unsigned char settings_start[]={0},settings_end[]={0};
int64_t esp_timer_get_time(void){return 10000000;}
uint32_t esp_random(void){return 123;}
void esp_fill_random(void *out,size_t n){memset(out,0x12,n);}
void esp_restart(void){}
void vTaskDelay(int t){}
int xTaskCreate(void(*f)(void*),const char*n,int stack,void*a,int prio,void*b){return 1;}
int esp_wifi_get_mac(int i,uint8_t *mac){memset(mac,2,6);return 0;}
unsigned room_audio_get_volume(void){return 100;}
bool room_audio_set_volume(unsigned v){return v<=100;}
bool room_audio_test(void){return true;}
bool room_mqtt_reannounce(void){return true;}
bool room_display_camera_fill(void){return false;}
void room_display_set_camera_fill(bool fill){}
cJSON *room_display_status(void){return cJSON_CreateObject();}
cJSON *room_wifi_status(void){return cJSON_CreateObject();}
bool room_wifi_submit(const char *ssid,const char *pw,bool open){return true;}
cJSON *room_controller_test(void){cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"message","Wi-Fi: controller acknowledged. ESP-NOW: controller acknowledged.");return j;}
cJSON *room_controller_status(void){return cJSON_CreateObject();}
int nvs_open(const char *name,int mode,unsigned *h){*h=1;return 0;}
void nvs_close(unsigned h){}
int nvs_get_blob(unsigned h,const char *key,void *out,size_t *size){if(!stored_size)return -1;if(*size<stored_size)return -1;memcpy(out,stored_profile,stored_size);*size=stored_size;return 0;}
int nvs_set_blob(unsigned h,const char *key,const void *in,size_t size){assert(size<=sizeof(stored_profile));memcpy(stored_profile,in,size);stored_size=size;return 0;}
int nvs_get_str(unsigned h,const char *key,char *out,size_t *size){if(!*stored_code)return -1;snprintf(out,*size,"%s",stored_code);*size=strlen(stored_code)+1;return 0;}
int nvs_set_str(unsigned h,const char *key,const char *s){snprintf(stored_code,sizeof(stored_code),"%s",s);return 0;}
int nvs_commit(unsigned h){return 0;}
int httpd_req_get_hdr_value_str(httpd_req_t *r,const char *name,char *out,size_t cap){for(unsigned i=0;i<8&&r->headers[i].name;++i)if(!strcmp(name,r->headers[i].name)){if(strlen(r->headers[i].value)>=cap)return -1;snprintf(out,cap,"%s",r->headers[i].value);return 0;}return -1;}
size_t httpd_req_get_hdr_value_len(httpd_req_t*r,const char*name){char value[256];return httpd_req_get_hdr_value_str(r,name,value,sizeof(value))==0?strlen(value):0;}
int httpd_req_recv(httpd_req_t *r,char *out,size_t cap){if(r->fail_at&&r->at>=r->fail_at)return -1;size_t n=r->content_len-r->at;if(n>cap)n=cap;if(r->fail_at&&r->at+(int)n>r->fail_at)n=r->fail_at-r->at;if(!n)return -1;memcpy(out,r->body+r->at,n);r->at+=n;return n;}
int httpd_resp_set_status(httpd_req_t*r,const char*s){r->code=atoi(s);return 0;}
int httpd_resp_set_type(httpd_req_t*r,const char*s){return 0;}
int httpd_resp_set_hdr(httpd_req_t*r,const char*a,const char*b){return 0;}
int httpd_resp_sendstr(httpd_req_t*r,const char*s){snprintf(r->response,sizeof(r->response),"%s",s);if(!r->code)r->code=200;return 0;}
int httpd_resp_send(httpd_req_t*r,const char*s,int n){return 0;}
int httpd_register_uri_handler(void*s,const httpd_uri_t*r){return 0;}
const esp_app_desc_t *esp_app_get_description(void){return &description;}
const esp_partition_t *esp_ota_get_running_partition(void){return &running;}
const esp_partition_t *esp_ota_get_next_update_partition(const void*p){return no_slot?NULL:&inactive;}
int esp_ota_get_state_partition(const esp_partition_t*p,int *state){*state=ESP_OTA_IMG_PENDING_VERIFY;return 0;}
int esp_ota_mark_app_valid_cancel_rollback(void){++confirms;return 0;}
int esp_ota_begin(const esp_partition_t*p,size_t n,unsigned*h){assert(p==&inactive&&n==OTA_WITH_SEQUENTIAL_WRITES);++begins;*h=42;return bad_begin?-1:0;}
int esp_ota_write(unsigned h,const void*p,size_t n){assert(h==42);++writes;return bad_write?-1:0;}
int esp_ota_end(unsigned h){return bad_end?-1:0;}
int esp_ota_abort(unsigned h){++aborts;return 0;}
int esp_ota_set_boot_partition(const esp_partition_t*p){assert(p==&inactive);if(bad_select)return -1;++selects;return 0;}
static httpd_req_t req(const char *body,size_t size,const char *type){return (httpd_req_t){.body=body,.content_len=size,.headers={{"Host","192.0.2.10"},{"Origin","http://192.0.2.10"},{"Content-Type",type}}};}
static void reset_upload(void){busy=false;reboot_pending=false;writes=selects=aborts=begins=0;bad_end=bad_write=bad_begin=bad_select=no_slot=false;}
int main(int argc,char **argv){
 old_settings_t old={.magic=OLD_MAGIC,.ha="https://ha.example",.token="keep-ha-token",.controller="http://192.0.2.20"};memcpy(stored_profile,&old,sizeof(old));stored_size=sizeof(old);room_settings_init();assert(!strcmp(current.token,"keep-ha-token"));assert(room_web_authorized(&(httpd_req_t){0},false));
 httpd_req_t r=req(NULL,0,"application/json");assert(room_web_authorized(&r,true));r.headers[1].value="http://evil.example";assert(!room_web_authorized(&r,true));
 r=req(NULL,0,"application/json");get(&r);assert(r.code==200&&!strstr(r.response,"keep-ha-token")&&!strstr(r.response,"controller_token")); /* Local configuration has no PIN gate. */
 const char *controller_only="{\"section\":\"controller\",\"controller\":\"http://192.0.2.21\"}";
 r=req(controller_only,strlen(controller_only),"application/json");save(&r);assert(r.code==200&&reboot_pending&&!strcmp(((settings_t*)stored_profile)->controller,"http://192.0.2.21"));reboot_pending=false;
 const char *mqtt="{\"section\":\"mqtt\",\"enabled\":true,\"broker\":\"mqtt://192.0.2.30:1883\",\"username\":\"ha\",\"password\":\"\",\"prefix\":\"homeassistant\",\"name\":\"Cottage\"}";
 r=req(mqtt,strlen(mqtt),"application/json");save(&r);assert(r.code==200);reboot_pending=false;memset(&current,0,sizeof(current));room_settings_init();cJSON *sync=room_settings_controller_integrations();cJSON *m=cJSON_GetObjectItem(sync,"mqtt");assert(!m);cJSON_Delete(sync); /* MQTT credentials are never forwarded to the controller. */
 const char *probe="{\"section\":\"controller_test\"}";size_t previous_size=stored_size;
 r=req(probe,strlen(probe),"application/json");save(&r);assert(r.code==200&&strstr(r.response,"acknowledged")&&!reboot_pending&&stored_size==previous_size);
 uint8_t image[4096]={0};esp_image_header_t header={.magic=ESP_IMAGE_HEADER_MAGIC,.chip_id=ESP_CHIP_ID_ESP32S3};esp_app_desc_t desc={.magic_word=ESP_APP_DESC_MAGIC_WORD,.project_name="cottage_thermo_room_4b"};
 memcpy(image,&header,sizeof(header));memcpy(image+sizeof(header)+sizeof(esp_image_segment_header_t),&desc,sizeof(desc));assert(valid_header(image,sizeof(image)));
 if(argc>1){FILE *file=fopen(argv[1],"rb");assert(file);uint8_t actual[1024];assert(fread(actual,1,sizeof(actual),file)==sizeof(actual));fclose(file);assert(valid_header(actual,sizeof(actual)));}
 reset_upload();r=req((char*)image,sizeof(image),"application/octet-stream");r.headers[1].value="http://evil.example";upload(&r);assert(r.code==403&&!begins&&!selects);
 reset_upload();image[0]=0;r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==400&&!begins&&!selects);image[0]=ESP_IMAGE_HEADER_MAGIC;
 reset_upload();header.chip_id=0;memcpy(image,&header,sizeof(header));r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==400&&!begins);header.chip_id=ESP_CHIP_ID_ESP32S3;memcpy(image,&header,sizeof(header));
 reset_upload();snprintf(desc.project_name,sizeof(desc.project_name),"controller");memcpy(image+sizeof(header)+sizeof(esp_image_segment_header_t),&desc,sizeof(desc));r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==400&&!begins);snprintf(desc.project_name,sizeof(desc.project_name),"cottage_thermo_room_4b");memcpy(image+sizeof(header)+sizeof(esp_image_segment_header_t),&desc,sizeof(desc));
 reset_upload();no_slot=true;r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==409&&!begins);
 reset_upload();r=req((char*)image,inactive.size+1,"application/octet-stream");upload(&r);assert(r.code==400&&!begins);
 reset_upload();r=req((char*)image,sizeof(image),"application/octet-stream");r.fail_at=1500;upload(&r);assert(r.code==400&&aborts==1&&!selects&&!busy);
 reset_upload();bad_write=true;r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==400&&aborts==1&&!selects);
 reset_upload();bad_end=true;r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==400&&!selects&&!aborts);
 reset_upload();bad_select=true;r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==400&&!selects);
 reset_upload();r=req((char*)image,sizeof(image),"application/octet-stream");upload(&r);assert(r.code==200&&selects==1&&busy&&reboot_pending);room_ota_confirm_boot();assert(confirms==1);
 puts("PASS: credential migration/redaction, local origin checks, MQTT keep-secret semantics and inactive-slot OTA validation/failure/activation/confirmation");
}

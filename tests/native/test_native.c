/* Real LVGL, mocked RTOS/network. No HA/device commands leave this process. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
static int64_t now=1000000;
static int posts=0,queue_count=0;
static char last_path[80],last_post[256];
static bool unknown=false,wifi_setup_requested=false;
static unsigned test_notify_mask,test_volume=100;
#include "../../touchscreen/main/room_display.c"
static command_t queued;
int64_t esp_timer_get_time(void){return now;}
uint32_t esp_random(void){return 123;}
int xSemaphoreTake(void *p,int n){return 1;}
int xSemaphoreGive(void *p){return 1;}
void *xSemaphoreCreateMutex(void){return (void*)1;}
void *xQueueCreate(int n,int size){return (void*)1;}
int xQueueSend(void *q,const void *c,int t){queued=*(command_t*)c;++queue_count;return 1;}
int xQueueReceive(void *q,void *c,int t){return 0;}
int uxQueueMessagesWaiting(void *q){return 0;}
int xTaskCreate(void(*f)(void*),const char*n,int a,void*b,int c,void*d){return 1;}
void vTaskDelay(int n){}
void vTaskDelete(void*p){}
void *heap_caps_malloc(size_t n,int cap){return malloc(n);}
bool bsp_display_lock(uint32_t n){return true;}
void bsp_display_unlock(void){}
lv_display_t *bsp_display_start(void){return lv_display_get_default();}
void bsp_display_rotate(lv_display_t *d,lv_display_rotation_t r){lv_display_set_rotation(d,r);}
int bsp_display_brightness_set(int n){return 0;}
void *bsp_i2c_get_handle(void){return (void*)47;}
const char *room_settings_controller_url(void){return CONFIG_ROOM_CONTROLLER_URL;}
unsigned room_audio_get_volume(void){return test_volume;}
bool room_audio_set_volume(unsigned value){if(value>100)return false;test_volume=value;return true;}
bool room_audio_test(void){return true;}
bool room_audio_notify_enabled(int key){return (test_notify_mask&(1U<<(key-3)))!=0;}
bool room_audio_set_notify(int key,bool enabled){unsigned bit=1U<<(key-3);test_notify_mask=enabled?test_notify_mask|bit:test_notify_mask&~bit;return true;}
void room_wifi_request_setup(void){wifi_setup_requested=true;}
bool room_wifi_setup_hint(char *buffer,size_t size){return false;}
static char ha_json[4096];
esp_err_t room_http(const char *origin,const char *header,const char *token,const char *path,const char *post,void *out,size_t cap,int timeout,int *status,int *length)
{
 const char *body;
 if(post){++posts;snprintf(last_path,sizeof(last_path),"%s",path);snprintf(last_post,sizeof(last_post),"%s",post);body=unknown?"{\"accepted\":false,\"outcome\":\"unknown\"}":"{\"accepted\":true}";}
 else if(!strcmp(path,"/api/ha/status"))body=ha_json;
 else body="{\"temperature_f\":65.2,\"target_f\":67,\"simulation\":true,\"mode\":\"hold\",\"reason\":\"simulation\"}";
 *length=snprintf(out,cap,"%s",body);*status=200;return 0;
}
cJSON *room_ha_status(void){return cJSON_Parse(ha_json);}
cJSON *room_ha_command(cJSON *p,int *code){char *post=cJSON_PrintUnformatted(p),buffer[256];int length;room_http("http://ha.test","Authorization","Bearer test","/api/ha/command",post,buffer,sizeof(buffer),1000,code,&length);free(post);return cJSON_Parse(buffer);}
static uint8_t screen_pixels[480*480*3],draw_pixels[480*480*3];
static void flush(lv_display_t *d,const lv_area_t *a,uint8_t *pixels)
{
 for(int y=a->y1;y<=a->y2;++y)memcpy(screen_pixels+(y*480+a->x1)*3,pixels+(y-a->y1)*(a->x2-a->x1+1)*3,(a->x2-a->x1+1)*3);
 lv_display_flush_ready(d);
}
static void render(const char *path)
{
 refresh(NULL);lv_refr_now(NULL);FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n480 480\n255\n");
 /* LVGL RGB888 stores BGR byte order. */
 for(int n=0;n<480*480;++n){fputc(screen_pixels[n*3+2],f);fputc(screen_pixels[n*3+1],f);fputc(screen_pixels[n*3],f);}fclose(f);
}
static void click(lv_obj_t *o){lv_obj_send_event(o,LV_EVENT_CLICKED,NULL);}
static lv_point_t point;
static bool pressed;
static void pointer_read(lv_indev_t *i,lv_indev_data_t *data){data->point=point;data->state=pressed?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;}
static void touch(int x,int y,bool down){point=(lv_point_t){x,y};pressed=down;lv_tick_inc(25);lv_timer_handler();}
static void populate(void)
{
 cJSON *root=cJSON_CreateObject();cJSON_AddBoolToObject(root,"connected",true);cJSON *array=cJSON_AddArrayToObject(root,"items");
 const char *status[]={"off","on","open","Running","Idle","Finished","Idle","off","65.2","docked","idle"};
 for(int k=0;k<COUNT;++k){cJSON *j=cJSON_CreateObject();cJSON_AddStringToObject(j,"key",keys[k]);cJSON_AddStringToObject(j,"state",status[k]);cJSON_AddNumberToObject(j,"sample_us",now);cJSON_AddBoolToObject(j,"available",true);cJSON *a=cJSON_AddArrayToObject(j,"actions");for(int n=0;n<6;++n)cJSON_AddItemToArray(a,cJSON_CreateString(actions[n]));if(k==8){cJSON_AddNumberToObject(j,"value",65.2);cJSON_AddStringToObject(j,"unit","F");}cJSON_AddItemToArray(array,j);}
 char *json=cJSON_PrintUnformatted(root);snprintf(ha_json,sizeof(ha_json),"%s",json);free(json);cJSON_Delete(root);
 poll_ha();poll_climate();
}
int main(void)
{
 lv_init();lv_display_t *d=lv_display_create(480,480);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB888);lv_display_set_buffers(d,draw_pixels,NULL,sizeof(draw_pixels),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flush);
 lv_indev_t *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,pointer_read);lv_indev_set_display(input,d);
 mutex=(void*)1;commands=(void*)1;build_ui();
 assert(lv_obj_has_state(control[0][0],LV_STATE_DISABLED));
 click(settings_button);assert(model.settings_open&&!lv_obj_has_flag(settings_modal,LV_OBJ_FLAG_HIDDEN));
 lv_slider_set_value(settings_volume,42,LV_ANIM_OFF);lv_obj_send_event(settings_volume,LV_EVENT_RELEASED,NULL);assert(test_volume==42);
 click(settings_camera_button);assert(model.camera_fill);
 click(settings_wifi_button);assert(wifi_setup_requested);
 click(settings_close_button);assert(!model.settings_open&&lv_obj_has_flag(settings_modal,LV_OBJ_FLAG_HIDDEN));
 populate();refresh(NULL);assert(!lv_obj_has_state(control[0][0],LV_STATE_DISABLED));
 assert(!strcmp(lv_label_get_text(temp_label),"65.2 F"));assert(!strcmp(lv_label_get_text(states[3]),"Running"));
 click(notify_button[0]);assert(room_audio_notify_enabled(3));assert(!room_audio_notify_enabled(4));
 click(notify_button[0]);assert(!room_audio_notify_enabled(3));
 int no_commands=queue_count;
 touch(400,365,true);touch(330,365,true);touch(240,365,true);touch(150,365,true);touch(80,365,true);touch(80,365,false);
 assert(visible_page==1);assert(queue_count==no_commands);
 /* Swiping from a control must navigate without submitting its click. */
 touch(310,70,true);touch(230,70,true);touch(150,70,true);touch(60,70,true);touch(60,70,false);
 assert(visible_page==2);assert(queue_count==no_commands);select_page(0);
 render("climate.ppm");select_page(1);render("home.ppm");select_page(2);render("laundry.ppm");select_page(3);render("camera.ppm");
 /* Different devices accept taps while one is in flight; only that row locks. */
 click(control[0][0]);command_t first=queued;
 refresh(NULL);assert(!strcmp(lv_label_get_text(states[0]),"on"));
 refresh(NULL);assert(lv_obj_has_state(control[0][0],LV_STATE_DISABLED));assert(!lv_obj_has_state(control[1][0],LV_STATE_DISABLED));
 click(control[1][0]);command_t second=queued;
 assert(first.item==0&&second.item==1&&(model.pending&3U)==3U);
 int queued_before=queue_count;click(control[0][0]);assert(queue_count==queued_before);
 execute(first);assert(!(model.pending&1U)&&(model.pending&2U));
 refresh(NULL);assert(!strcmp(lv_label_get_text(states[0]),"on"));
 execute(second);assert(model.pending==0);posts=0;
 assert(lv_obj_get_height(body[1])==364);
 assert(lv_obj_has_flag(notice,LV_OBJ_FLAG_HIDDEN));
 assert(bulb_head[0]&&bulb_head[1]&&activity_ring[3]&&activity_ring[6]);
 assert(room_visual_state("Washing",true)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("Not in Use",true)==ROOM_VIS_IDLE);
 /* Kids input_boolean stays on the HA command path. No furnace mutation. */
 click(control[7][0]);assert(queued.item==7&&queued.action==0);assert(model.pending);execute(queued);assert(posts==1);assert(!strcmp(last_path,"/api/ha/command"));assert(strstr(last_post,"kids_heat"));
 unknown=true;click(control[9][3]);execute(queued);assert(strstr(model.notice,"Outcome unknown"));unknown=false;
 /* Stale requests expire rather than replay after network work. */
 click(control[0][0]);int before=posts;now+=11000000;execute(queued);assert(posts==before);assert(strstr(model.notice,"expired"));
 now+=11000000;refresh(NULL);assert(lv_obj_has_state(control[0][0],LV_STATE_DISABLED));int count=queue_count;click(control[0][0]);assert(queue_count==count);assert(!camera_current(model.generation));
 populate();click(plus_button);click(apply_button);assert(queued.item==-1&&queued.target==67.5f);execute(queued);assert(!strcmp(last_path,"/api/config"));assert(strstr(last_post,"67.5"));
 assert(room_display_probe_bus(1,2)==NULL);assert(room_display_probe_bus(47,48)==(void*)47);
 select_page(2);now+=61000000;refresh(NULL);assert(visible_page==0);assert(!camera_current(model.generation));
 puts("PASS: native LVGL pages/swipes, HA commands, unknown outcomes, expired requests, stale controls, target-only climate, probe guard and idle return");
 return 0;
}

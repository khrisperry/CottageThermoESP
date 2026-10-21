/* Real LVGL, mocked RTOS/network. No HA/device commands leave this process. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host.h"
#define CONFIG_ROOM_SENSOR 3
static int64_t now=1000000;
static int posts=0,queue_count=0;
static char last_path[80],last_post[256];
static bool unknown=false,test_wifi=true,test_fallback=false;
static bool ship_showing,test_ships_populated,test_ship_map_pending;
static unsigned test_notify_mask,test_volume=100;
#include "../../touchscreen/main/room_display.c"
/* Worker tasks are registered but not run in this deterministic UI fixture. */
esp_err_t room_ha_camera(void *buffer,size_t capacity,int *code,int *length){assert(!"Camera worker must not run in UI fixture");return ESP_FAIL;}
esp_err_t esp_jpeg_decode(esp_jpeg_image_cfg_t *cfg,esp_jpeg_image_output_t *out){assert(!"JPEG worker must not run in UI fixture");return ESP_FAIL;}
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
lv_display_t *bsp_display_start_with_config(const bsp_display_cfg_t *cfg){assert(cfg->lvgl_port_cfg.task_stack==16384);return lv_display_get_default();}
unsigned uxTaskGetStackHighWaterMark(void *task){return 12000;}
void bsp_display_rotate(lv_display_t *d,lv_display_rotation_t r){lv_display_set_rotation(d,r);}
int bsp_display_brightness_set(int n){return 0;}
void *bsp_i2c_get_handle(void){return (void*)47;}
bool room_wifi_connected(void){return test_wifi;}
const char *room_settings_ha_token(void){return "configured";}
bool room_controller_fallback(void){return test_fallback;}
bool room_controller_radio_ready(void){return true;}
bool room_controller_radio_online(void){return test_fallback;}
const char *room_settings_controller_token(void){return "0123456789abcdef0123456789abcdef";}
const char *room_settings_controller_url(void){return CONFIG_ROOM_CONTROLLER_URL;}
unsigned room_audio_get_volume(void){return test_volume;}
bool room_audio_set_volume(unsigned value){if(value>100)return false;test_volume=value;return true;}
bool room_audio_test(void){return true;}
bool room_audio_notify_enabled(int key){return (test_notify_mask&(1U<<(key-3)))!=0;}
bool room_audio_set_notify(int key,bool enabled){unsigned bit=1U<<(key-3);test_notify_mask=enabled?test_notify_mask|bit:test_notify_mask&~bit;return true;}
bool room_ota_busy(void){return false;}
const char *room_settings_admin_code(void){return "testadmin123";}
void room_wifi_address(char *buffer,size_t size){snprintf(buffer,size,"%s","192.0.2.10");}
bool room_settings_ships_enabled(void){return true;}
unsigned room_settings_ship_range(void){return 30;}
int room_controller_mqtt_state(void){return 0;}
const char *room_controller_sync_state(void){return "Not managed";}
void room_ships_active(bool active){ship_showing=active;}
void room_ships_snapshot(room_ships_t *out){memset(out,0,sizeof(*out));out->service=true;out->feed=true;out->received_at=now;out->range=30;out->latitude=44.3f;out->longitude=-83.5f;
 if(test_ships_populated){out->count=out->nearby=3;for(unsigned i=0;i<3;++i){room_ship_t *v=&out->ships[i];v->mmsi=316000001+i;snprintf(v->name,sizeof(v->name),"TEST VESSEL %u",i+1);snprintf(v->kind,sizeof(v->kind),"Cargo");snprintf(v->destination,sizeof(v->destination),"PORT HURON");v->lat=44.3f+i*.02f;v->lon=-83.4f+i*.02f;v->distance=3+i;v->speed_valid=true;v->speed=11.2f;v->age=30;}}
}
bool room_ships_online(void){return true;}
void *room_ships_take_map(double bounds[4]){
 if(!test_ship_map_pending)return NULL;test_ship_map_pending=false;
 bounds[0]=-84;bounds[1]=44;bounds[2]=-83;bounds[3]=44.6;
 uint16_t *p=malloc(456*400*2);assert(p);for(unsigned y=0;y<400;++y)for(unsigned x=0;x<456;++x)p[y*456+x]=(x%57==0||y%50==0)?0x22aa:0x1105;return p;
}
size_t heap_caps_get_free_size(int caps){return 1000000;}
size_t heap_caps_get_largest_free_block(int caps){return 100000;}
static char ha_json[4096];
esp_err_t room_http(const char *origin,const char *header,const char *token,const char *path,const char *post,void *out,size_t cap,int timeout,int *status,int *length)
{
 const char *body;
 if(post){++posts;snprintf(last_path,sizeof(last_path),"%s",path);snprintf(last_post,sizeof(last_post),"%s",post);body=unknown?"{\"accepted\":false,\"outcome\":\"unknown\"}":"{\"accepted\":true}";}
 else if(!strcmp(path,"/api/ha/status"))body=ha_json;
 else body="{\"temperature_f\":65.2,\"target_f\":67,\"simulation\":true,\"mode\":\"hold\",\"reason\":\"simulation\"}";
 *length=snprintf(out,cap,"%s",body);*status=200;return 0;
}
cJSON *room_controller_status(void){char buffer[1024];int code,n;room_http("http://controller.test","X-Thermo-Token","test","/api/status",NULL,buffer,sizeof(buffer),1000,&code,&n);return cJSON_Parse(buffer);}
cJSON *room_controller_target(float target,int *code){char buffer[1024],post[80];int n;snprintf(post,sizeof(post),"{\"target_f\":%.1f}",target);room_http("http://controller.test","X-Thermo-Token","test","/api/config",post,buffer,sizeof(buffer),1000,code,&n);return cJSON_Parse(buffer);}
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
 room_display_start();
 for(unsigned i=0;i<4;++i)refresh(NULL);assert(lvgl_stack_free==12000);
 assert(lv_obj_has_state(control[0][0],LV_STATE_DISABLED));
 click(settings_button);assert(model.settings_open&&!lv_obj_has_flag(settings_modal,LV_OBJ_FLAG_HIDDEN));
 lv_slider_set_value(settings_volume,42,LV_ANIM_OFF);lv_obj_send_event(settings_volume,LV_EVENT_RELEASED,NULL);assert(test_volume==42);
 click(settings_camera_button);assert(model.camera_fill);
 click(settings_close_button);assert(!model.settings_open&&lv_obj_has_flag(settings_modal,LV_OBJ_FLAG_HIDDEN));
 populate();refresh(NULL);assert(!lv_obj_has_state(control[0][0],LV_STATE_DISABLED));
 assert(!strcmp(lv_label_get_text(connection),"Online"));
 assert(!strcmp(lv_label_get_text(lv_obj_get_child(settings_button,0)),LV_SYMBOL_SETTINGS));
 click(health_button);refresh(NULL);assert(model.settings_open);assert(!strcmp(lv_label_get_text(diagnostic_cells[3]),"Standby"));
 assert(lv_obj_get_x(diagnostic_badges[0])==lv_obj_get_x(diagnostic_badges[6]));
 assert(lv_obj_get_x(diagnostic_cells[0])==lv_obj_get_x(diagnostic_cells[6]));
 render("connections.ppm");click(diagnostic_close_button);assert(!model.settings_open);
 test_fallback=true;test_wifi=false;refresh(NULL);
 assert(strstr(lv_label_get_text(connection),"ESPNOW"));assert(lv_obj_has_state(control[0][0],LV_STATE_DISABLED));
 int before_fallback=queue_count;click(control[0][0]);assert(before_fallback==queue_count);assert(!camera_current(model.generation));
 render("fallback.ppm");test_fallback=false;test_wifi=true;populate();refresh(NULL);
 room_display_probe_sample(65.2f);refresh(NULL);
 assert(!strcmp(lv_label_get_text(temp_label),"65.2 F"));assert(!strcmp(model.items[3].state,"Running")); /* Hidden long-dot labels are laid out when shown. */
 click(notify_button[0]);assert(room_audio_notify_enabled(3));assert(!room_audio_notify_enabled(4));
 click(notify_button[0]);assert(!room_audio_notify_enabled(3));
 int no_commands=queue_count;
 touch(400,365,true);touch(330,365,true);touch(240,365,true);touch(150,365,true);touch(80,365,true);touch(80,365,false);
 assert(visible_page==1);assert(queue_count==no_commands);
 /* Swiping from a control must navigate without submitting its click. */
 touch(310,70,true);touch(230,70,true);touch(150,70,true);touch(60,70,true);touch(60,70,false);
 assert(visible_page==2);assert(queue_count==no_commands);select_page(0);
 render("climate.ppm");select_page(1);render("home.ppm");
 assert(blind_slats[0]&&heater_fins[0]&&!icon_badge[2]&&!icon_badge[7]);
 assert(lv_obj_get_style_text_font(states[8],LV_PART_MAIN)==&lv_font_montserrat_28);
 assert(lv_obj_get_y(blind_slats[4])==16);assert(lv_obj_has_flag(heater_rays[0],LV_OBJ_FLAG_HIDDEN));
 snprintf(model.items[2].state,sizeof(model.items[2].state),"closed");
 snprintf(model.items[7].state,sizeof(model.items[7].state),"on");render("home-heating.ppm");
 assert(lv_obj_get_y(blind_slats[4])==36);assert(!lv_obj_has_flag(heater_rays[0],LV_OBJ_FLAG_HIDDEN));
 populate();refresh(NULL);
select_page(2);render("laundry.ppm");select_page(3);render("camera.ppm");
 /* Render decoded frames through the production presenter, including upscaled
  * Fit/Fill and repeated cache replacement. Empty-camera renders missed the mask allocation. */
 assert(!lv_obj_get_style_clip_corner(camera_frame,LV_PART_MAIN));
 assert(lv_obj_get_style_radius(camera_frame,LV_PART_MAIN)==0);
 for(unsigned frame=0;frame<6;++frame){
     uint16_t *pixels=malloc(320*180*2);assert(pixels);
     for(unsigned i=0;i<320*180;++i)pixels[i]=0x07e0;
     camera_present(pixels,320,180,320*180*2,(frame&1)!=0);
     assert(!lv_obj_has_flag(camera_image,LV_OBJ_FLAG_HIDDEN));
     render((frame&1)?"camera-fill.ppm":"camera-fit.ppm");
     assert(screen_pixels[(240*480+240)*3+1]>128);
 }
 select_page(0);
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
 select_page(4);assert(ship_showing);refresh(NULL);assert(strstr(lv_label_get_text(ship_detail),"No ships"));
 render("ships-empty.ppm");test_ships_populated=true;test_ship_map_pending=true;refresh(NULL);lv_obj_update_layout(lv_screen_active());
 assert(!lv_obj_has_flag(ship_map,LV_OBJ_FLAG_HIDDEN));assert(!lv_obj_has_flag(ship_markers[0],LV_OBJ_FLAG_HIDDEN));
 assert(strstr(lv_label_get_text(ship_detail),"3.0 mi"));render("ships-map.ppm");lv_mem_monitor_t before_map,after_map;lv_mem_monitor(&before_map);
 for(unsigned i=0;i<100;++i){test_ship_map_pending=true;refresh(NULL);lv_refr_now(NULL);}
 lv_mem_monitor(&after_map);assert(after_map.free_size+1024>=before_map.free_size);
 click(ship_markers[1]);assert(selected_ship==1);test_ships_populated=false;
 select_page(0);assert(!ship_showing);
 cJSON *device=room_display_status();assert(cJSON_GetArraySize(cJSON_GetObjectItem(device,"services"))==7);cJSON_Delete(device);
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
 /* Local readings remain visible without controller or HA, but stale readings clear. */
 model.climate_ok=false;model.ha_ok=false;room_display_probe_sample(69.4f);refresh(NULL);
 assert(!strcmp(lv_label_get_text(temp_label),"69.4 F"));
 assert(lv_obj_has_state(apply_button,LV_STATE_DISABLED));
 click(settings_button);refresh(NULL);assert(strstr(lv_label_get_text(probe_test_label),"69.4 F"));render("bmp280-settings.ppm");
 now+=31000000;refresh(NULL);assert(!strcmp(lv_label_get_text(temp_label),"-- F"));
 assert(strstr(lv_label_get_text(probe_test_label),"no fresh reading"));
 puts("PASS: native LVGL pages/swipes, HA commands, unknown outcomes, expired requests, stale controls, target-only climate, probe guard and idle return");
 return 0;
}


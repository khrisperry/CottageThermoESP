/* Native 480x480 Waveshare 4B panel. Network tasks never hold the LVGL lock
 * while doing I/O. Furnace arming/mode/servo endpoints are deliberately absent. */
#include "room_display.h"
#include "sdkconfig.h"
#if CONFIG_ROOM_NATIVE_DISPLAY
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "bsp/esp-bsp.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "jpeg_decoder.h"
#include "room_transport.h"
#include "room_ha.h"
#include "wifi_setup.h"

#define COUNT 11
#define FRESH_US 10000000LL
static const char *TAG="room_display";
static const char *keys[COUNT]={"deck","breezeway","blinds","washer1","washer2","dryer1","dryer2","kids_heat","kids_temp","vacuum","camera"};
static const char *actions[]={"on","off","open","stop","close","start"};
static const char *pages[]={"Climate","Home","Laundry","Camera"};
typedef struct { bool available; unsigned actions; char state[128]; } item_t;
typedef struct {
    item_t items[COUNT]; int64_t ha_at,climate_at;
    bool ha_ok,climate_ok,has_temp,has_target,simulation,pending;
    float temp,target; char mode[32],reason[80],notice[128],ha_error[128];
    unsigned page,generation;
} model_t;
typedef struct { int item,action; float target; int64_t at; } command_t;
static model_t model;
static SemaphoreHandle_t mutex;
static QueueHandle_t commands;
static lv_obj_t *body[4],*nav[4],*connection,*notice,*temp_label,*target_label,*mode_label,*reason_label;
static lv_obj_t *states[COUNT],*control[COUNT][6],*minus_button,*plus_button,*apply_button,*camera_image,*camera_note,*wifi_button;
static lv_image_dsc_t image_desc;
static void *image_pixels;
static float draft=65;
static bool editing;
static int64_t last_touch;
static unsigned visible_page;

static model_t snapshot(void) { model_t m; xSemaphoreTake(mutex,portMAX_DELAY);m=model;xSemaphoreGive(mutex);return m; }
static void message(const char *text) { xSemaphoreTake(mutex,portMAX_DELAY);snprintf(model.notice,sizeof(model.notice),"%s",text);xSemaphoreGive(mutex); }
static const char *str(cJSON *j,const char *key) { cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:""; }
static bool number(cJSON *j,const char *key,float *out) { cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;*out=v->valuedouble;return true; }
static bool yes(cJSON *j,const char *key) { return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,key)); }
static cJSON *json_request(bool ha,const char *path,const char *post,int *code)
{
    if(ha){
        if(!post){*code=200;return room_ha_status();}
        cJSON *payload=cJSON_Parse(post);cJSON *result=room_ha_command(payload,code);cJSON_Delete(payload);if(*code!=200){cJSON_Delete(result);return NULL;}return result;
    }
    *code=0;char *buffer=malloc(8192);if(!buffer)return NULL;
    int n;esp_err_t err=room_http(CONFIG_ROOM_CONTROLLER_URL,"X-Thermo-Token",CONFIG_ROOM_API_TOKEN,
        path,post,buffer,8192,post?20000:3000,code,&n);
    cJSON *j=err==ESP_OK&&*code==200?cJSON_ParseWithLengthOpts(buffer,n+1,NULL,true):NULL;
    free(buffer);if(!cJSON_IsObject(j)){cJSON_Delete(j);return NULL;}return j;
}
static void poll_ha(void)
{
    int code; cJSON *j=json_request(true,"/api/ha/status",NULL,&code);
    cJSON *array=cJSON_GetObjectItemCaseSensitive(j,"items");
    item_t items[COUNT]={0};bool valid=cJSON_IsArray(array);
    if(valid)for(int k=0;k<COUNT;++k) {
        cJSON *entry;
        cJSON_ArrayForEach(entry,array)if(!strcmp(str(entry,"key"),keys[k])) {
            item_t *i=&items[k];i->available=yes(entry,"available");
            float value;
            if(k==8 && number(entry,"value",&value))snprintf(i->state,sizeof(i->state),"%.1f %s",value,str(entry,"unit"));
            else snprintf(i->state,sizeof(i->state),"%s",str(entry,"state"));
            cJSON *a,*list=cJSON_GetObjectItemCaseSensitive(entry,"actions");
            cJSON_ArrayForEach(a,list)if(cJSON_IsString(a))for(int n=0;n<6;++n)if(!strcmp(a->valuestring,actions[n]))i->actions|=1U<<n;
            break;
        }
    }
    xSemaphoreTake(mutex,portMAX_DELAY);
    snprintf(model.ha_error,sizeof(model.ha_error),"%s",str(j,"error"));
    memcpy(model.items,items,sizeof(items));model.ha_ok=valid&&yes(j,"connected");model.ha_at=valid?esp_timer_get_time():0;
    xSemaphoreGive(mutex);cJSON_Delete(j);
}
static void poll_climate(void)
{
    int code;cJSON *j=json_request(false,"/api/status",NULL,&code);float t=0,target=0;
    bool ht=number(j,"temperature_f",&t),hs=number(j,"target_f",&target)&&target>=45&&target<=80;
    xSemaphoreTake(mutex,portMAX_DELAY);
    model.climate_ok=j!=NULL;model.climate_at=j?esp_timer_get_time():0;
    model.has_temp=ht;model.temp=t;model.has_target=hs;model.target=target;
    model.simulation=yes(j,"simulation");
    snprintf(model.mode,sizeof(model.mode),"%s",str(j,"mode"));
    snprintf(model.reason,sizeof(model.reason),"%s",str(j,"reason"));
    xSemaphoreGive(mutex);cJSON_Delete(j);
}
static bool current(int64_t at) { return at>0 && esp_timer_get_time()-at<FRESH_US; }
static void execute(command_t c)
{
    model_t m=snapshot();bool allowed=esp_timer_get_time()-c.at<2000000;
    if(c.item<0)allowed=allowed&&m.climate_ok&&m.has_target&&current(m.climate_at)&&c.target>=45&&c.target<=80;
    else allowed=allowed&&current(m.ha_at)&&m.items[c.item].available&&(m.items[c.item].actions&(1U<<c.action));
    if(!allowed)message("Request expired or device unavailable; nothing sent");
    else {
        char payload[200];int code;
        if(c.item<0)snprintf(payload,sizeof(payload),"{\"target_f\":%.1f}",c.target);
        else snprintf(payload,sizeof(payload),"{\"key\":\"%s\",\"action\":\"%s\",\"request_id\":\"esp-%08" PRIx32 "-%08" PRIx32 "-%" PRId64 "\"}",keys[c.item],actions[c.action],esp_random(),esp_random(),c.at);
        cJSON *j=json_request(c.item>=0,c.item<0?"/api/config":"/api/ha/command",payload,&code);
        if(!j)message(code>=400?"Request rejected; check state before retrying":"Connection lost; outcome unknown. Check device state");
        else if(c.item>=0&&!yes(j,"accepted"))message("Outcome unknown; check device state before retrying");
        else message(c.item<0?"Target saved; checking controller":"Service completed; checking device state");
        cJSON_Delete(j);
        if(c.item<0)poll_climate();else poll_ha();
    }
    xSemaphoreTake(mutex,portMAX_DELAY);model.pending=false;xSemaphoreGive(mutex);
}
static void network_task(void *arg)
{
    int64_t ha_due=0,climate_due=0;
    for(;;) {
        command_t c;
        if(xQueueReceive(commands,&c,pdMS_TO_TICKS(100))==pdTRUE){execute(c);continue;}
        int64_t now=esp_timer_get_time();
        /* A slow request cannot block touch or replay an old command. */
        if(now>=ha_due){poll_ha();ha_due=esp_timer_get_time()+3000000;}
        if(uxQueueMessagesWaiting(commands))continue;
        if(now>=climate_due){poll_climate();climate_due=esp_timer_get_time()+5000000;}
    }
}
static bool camera_current(unsigned generation)
{
    model_t m=snapshot();return m.page==3 && m.generation==generation && current(m.ha_at)&&m.items[10].available;
}
static void camera_task(void *arg)
{
    uint8_t *jpeg=heap_caps_malloc(ROOM_HA_CAMERA_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!jpeg){message("Camera memory unavailable");vTaskDelete(NULL);return;}
    for(;;) {
        model_t m=snapshot();
        if(camera_current(m.generation)) {
            int code,n;
            esp_err_t err=room_ha_camera(jpeg,ROOM_HA_CAMERA_CAPACITY,&code,&n);
            esp_jpeg_image_cfg_t cfg={.indata=jpeg,.indata_size=n,.out_format=JPEG_IMAGE_FORMAT_RGB565,.out_scale=JPEG_IMAGE_SCALE_0};
            esp_jpeg_image_output_t info={0};void *pixels=NULL;
            if(err==ESP_OK&&code==200&&esp_jpeg_get_image_info(&cfg,&info)==ESP_OK&&info.width&&info.height&&info.width<=480&&info.height<=320&&info.output_len<=480*320*2) {
                pixels=heap_caps_malloc(info.output_len,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
                cfg.outbuf=pixels;cfg.outbuf_size=info.output_len;
                if(!pixels||esp_jpeg_decode(&cfg,&info)!=ESP_OK){free(pixels);pixels=NULL;}
            }
            if(bsp_display_lock(1000)) {
                if(camera_current(m.generation)) {
                    lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
                    if(pixels) {
                        lv_image_cache_drop(&image_desc);
                        free(image_pixels);image_pixels=pixels;pixels=NULL;
                        image_desc=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
                            .w=info.width,.h=info.height,.stride=info.width*2},.data_size=info.output_len,.data=image_pixels};
                        lv_image_set_src(camera_image,&image_desc);
                        uint32_t scale=256;
                        if(info.width>456)scale=456*256/info.width;
                        if(info.height*scale/256>294)scale=294*256/info.height;
                        lv_image_set_scale(camera_image,scale);lv_obj_align(camera_image,LV_ALIGN_CENTER,0,-16);
                        lv_obj_remove_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
                        lv_label_set_text(camera_note,"Live view - up to 1 frame/second");
                    } else lv_label_set_text(camera_note,"Camera unavailable - old image cleared");
                }
                bsp_display_unlock();
            }
            free(pixels);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
static void enabled(lv_obj_t *b,bool value) { if(value)lv_obj_remove_state(b,LV_STATE_DISABLED);else lv_obj_add_state(b,LV_STATE_DISABLED); }
static void select_page(unsigned p)
{
    visible_page=p;last_touch=esp_timer_get_time();
    for(unsigned n=0;n<4;++n){if(n==p)lv_obj_remove_flag(body[n],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(body[n],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(nav[n],lv_color_hex(n==p?0xa5e5cc:0x254652),0);lv_obj_set_style_text_color(nav[n],lv_color_hex(n==p?0x102832:0xecf5f4),0);}
    xSemaphoreTake(mutex,portMAX_DELAY);model.page=p;++model.generation;xSemaphoreGive(mutex);
    lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(camera_note,"Loading live view...");
}
static void wifi_setup_event(lv_event_t *e){last_touch=esp_timer_get_time();room_wifi_request_setup();}
static void nav_event(lv_event_t *e) {select_page((unsigned)(uintptr_t)lv_event_get_user_data(e));}
static void gesture(lv_event_t *e)
{
    lv_indev_t *input=lv_indev_active();if(!input)return;
    lv_dir_t d=lv_indev_get_gesture_dir(input);
    if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT){select_page((visible_page+(d==LV_DIR_LEFT?1:3))%4);lv_indev_wait_release(input);}
}
static void send_event(lv_event_t *e)
{
    intptr_t id=(intptr_t)lv_event_get_user_data(e);model_t m=snapshot();last_touch=esp_timer_get_time();
    if(m.pending)return;
    if(id==-2||id==-3){if(m.climate_ok&&m.has_target&&current(m.climate_at)){draft=fmaxf(45,fminf(80,draft+(id==-2?-.5f:.5f)));editing=true;}return;}
    command_t c={.item=id<0?-1:id/6,.action=id<0?0:id%6,.target=draft,.at=esp_timer_get_time()};
    if(c.item<0){if(!m.climate_ok||!m.has_target||!current(m.climate_at))return;}
    else if(!current(m.ha_at)||!m.items[c.item].available||!(m.items[c.item].actions&(1U<<c.action)))return;
    xSemaphoreTake(mutex,portMAX_DELAY);model.pending=true;snprintf(model.notice,sizeof(model.notice),"Sending request...");xSemaphoreGive(mutex);
    if(xQueueSend(commands,&c,0)!=pdTRUE){xSemaphoreTake(mutex,portMAX_DELAY);model.pending=false;xSemaphoreGive(mutex);message("Busy; request not queued");}
    editing=false;
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int width,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(o,font,0);lv_label_set_text(o,text);return o;
}
static lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int width,int height,lv_event_cb_t cb,intptr_t id)
{
    lv_obj_t *o=lv_button_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,height);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xa5e5cc),0);lv_obj_set_style_text_color(o,lv_color_hex(0x102832),0);
    lv_obj_set_style_radius(o,10,0);lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void*)id);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,text);lv_obj_center(l);return o;
}
static void refresh(lv_timer_t *timer)
{
    model_t m=snapshot();int64_t now=esp_timer_get_time();
    if(visible_page&&now-last_touch>60000000){select_page(0);m=snapshot();}
    bool ha=m.ha_ok&&current(m.ha_at),climate=m.climate_ok&&current(m.climate_at);
    lv_label_set_text_fmt(connection,"HA %s  |  Heat %s",ha?"online":"offline",climate?"online":"offline");
    char wifi_hint[96];
    lv_label_set_text(notice,room_wifi_setup_hint(wifi_hint,sizeof(wifi_hint))?wifi_hint:(*m.notice?m.notice:m.ha_error));
    if(!editing&&climate&&m.has_target)draft=m.target;
    char text[80];
    if(climate&&m.has_temp){snprintf(text,sizeof(text),"%.1f F",m.temp);lv_label_set_text(temp_label,text);}else lv_label_set_text(temp_label,"-- F");
    if(climate&&m.has_target){snprintf(text,sizeof(text),"%s %.1f F",editing?"Set to":"Target",draft);lv_label_set_text(target_label,text);}else lv_label_set_text(target_label,"Target unavailable");
    lv_label_set_text_fmt(mode_label,"%s%s",climate?m.mode:"Controller offline",climate?(m.simulation?" - simulation":" - bench mode"):"");
    lv_label_set_text(reason_label,climate?m.reason:"Home controls still work when HA is online");
    enabled(minus_button,climate&&m.has_target&&!m.pending);enabled(plus_button,climate&&m.has_target&&!m.pending);enabled(apply_button,climate&&m.has_target&&!m.pending);
    for(int k=0;k<COUNT;++k){bool available=ha&&m.items[k].available;
        if(states[k])lv_label_set_text(states[k],available?m.items[k].state:"Unavailable");
        for(int a=0;a<6;++a)if(control[k][a])enabled(control[k][a],available&&!m.pending&&(m.items[k].actions&(1U<<a)));}
    if(!ha||!m.items[10].available)lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
}
static void control_button(lv_obj_t *p,const char *text,int key,int action,int x,int y,int w)
{control[key][action]=button(p,text,x,y,w,44,send_event,key*6+action);enabled(control[key][action],false);}
static void build_ui(void)
{
    lv_obj_t *screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(0x102832),0);
    lv_obj_set_style_text_color(screen,lv_color_hex(0xecf5f4),0);lv_obj_set_style_text_font(screen,&lv_font_montserrat_16,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(screen,gesture,LV_EVENT_GESTURE,NULL);
    label(screen,"Cottage",16,9,150,&lv_font_montserrat_24);
    connection=label(screen,"Connecting...",155,16,225,&lv_font_montserrat_14);
    wifi_button=button(screen,"Wi-Fi",390,7,78,34,wifi_setup_event,0);
    for(int n=0;n<4;++n){body[n]=lv_obj_create(screen);lv_obj_remove_style_all(body[n]);lv_obj_set_pos(body[n],12,49);lv_obj_set_size(body[n],456,333);lv_obj_remove_flag(body[n],LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_flag(body[n],LV_OBJ_FLAG_GESTURE_BUBBLE);
        nav[n]=button(screen,pages[n],12+n*116,429,108,44,nav_event,n);}
    notice=label(screen,"Waiting for first update",16,388,448,&lv_font_montserrat_14);
    label(body[0],"ROOM TEMPERATURE",0,0,456,&lv_font_montserrat_16);
    temp_label=label(body[0],"-- F",0,35,456,&lv_font_montserrat_48);
    target_label=label(body[0],"Target unavailable",0,110,456,&lv_font_montserrat_28);
    minus_button=button(body[0],"-",0,162,100,58,send_event,-2);apply_button=button(body[0],"Set target",112,162,230,58,send_event,-1);plus_button=button(body[0],"+",354,162,100,58,send_event,-3);
    mode_label=label(body[0],"Controller offline",0,245,456,&lv_font_montserrat_20);
    reason_label=label(body[0],"Waiting for status",0,278,456,&lv_font_montserrat_16);
    const int rows[]={0,1,2,7,9};const char *names[]={"Deck lights","Breezeway","Blinds","Kids heat","Vacuum"};
    for(int n=0;n<5;++n){int key=rows[n],y=n*64;label(body[1],names[n],0,y,218,&lv_font_montserrat_20);states[key]=label(body[1],"Unavailable",0,y+27,218,&lv_font_montserrat_14);
        if(key==2){control_button(body[1],"Open",key,2,222,y,74);control_button(body[1],"Stop",key,3,302,y,74);control_button(body[1],"Close",key,4,382,y,74);}
        else if(key==9){control_button(body[1],"Start",key,5,246,y,96);control_button(body[1],"Stop",key,3,354,y,96);}
        else {control_button(body[1],"On",key,0,246,y,96);control_button(body[1],"Off",key,1,354,y,96);}}
    /* Kids temperature stays a supporting display-only line, not the furnace input. */
    states[8]=label(body[1],"Unavailable",113,3*64+27,116,&lv_font_montserrat_14);lv_obj_set_width(states[7],108);
    const char *laundry[]={"Washer 1","Washer 2","Dryer 1","Dryer 2"};
    for(int n=0;n<4;++n){int x=(n%2)*232,y=(n/2)*146;label(body[2],laundry[n],x,y+12,220,&lv_font_montserrat_24);states[n+3]=label(body[2],"Unavailable",x,y+54,214,&lv_font_montserrat_20);}
    label(body[2],"Reported status - no estimated cycle times",0,308,456,&lv_font_montserrat_14);
    camera_image=lv_image_create(body[3]);lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
    camera_note=label(body[3],"Loading live view...",0,309,456,&lv_font_montserrat_14);
    lv_timer_create(refresh,250,NULL);select_page(0);refresh(NULL);
}
void room_display_start(void)
{
    mutex=xSemaphoreCreateMutex();commands=xQueueCreate(1,sizeof(command_t));
    ESP_ERROR_CHECK(mutex&&commands?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(bsp_display_start()?ESP_OK:ESP_FAIL);
    if(bsp_display_lock(0)){
        #if CONFIG_ROOM_DISPLAY_ROTATE_180
        bsp_display_rotate(lv_display_get_default(),LV_DISPLAY_ROTATION_180);
#endif
        build_ui();bsp_display_unlock();
    }
    ESP_ERROR_CHECK(bsp_display_brightness_set(80));
    ESP_ERROR_CHECK(xTaskCreate(network_task,"room_network",10240,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(camera_task,"room_camera",8192,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_LOGI(TAG,"Native 4B ready: Climate / Home / Laundry / Camera");
}
i2c_master_bus_handle_t room_display_probe_bus(int sda,int scl)
{
    if(sda!=BSP_I2C_SDA||scl!=BSP_I2C_SCL){ESP_LOGE(TAG,"4B external probe must share board I2C SDA47/SCL48; probe disabled");return NULL;}
    return bsp_i2c_get_handle();
}
#else
void room_display_start(void) {}
i2c_master_bus_handle_t room_display_probe_bus(int sda,int scl) {return NULL;}
#endif

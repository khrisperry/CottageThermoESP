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
#include <limits.h>
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
#include "room_settings.h"
#include "room_camera_size.h"
#include "room_camera_scale.h"
#include "room_visual_state.h"
#include "room_feedback.h"
#include "room_audio.h"
#include "room_controller.h"
#include "room_health.h"
#include "wifi_setup.h"
#include "room_ships.h"
#include "room_ota.h"
#include "room_mqtt.h"
#include <stdatomic.h>

#define COUNT 11
#define FRESH_US 10000000LL
#define COMMAND_TTL_US 10000000LL
#define CLIMATE_PENDING (1U<<COUNT)
static const char *TAG="room_display";
static const char *keys[COUNT]={"deck","breezeway","blinds","washer1","washer2","dryer1","dryer2","kids_heat","kids_temp","vacuum","camera"};
static const char *actions[]={"on","off","open","stop","close","start"};
typedef struct { bool available; unsigned actions; int64_t sampled; char state[128]; } item_t;
typedef struct {
    item_t items[COUNT];room_optimistic_t optimistic[COUNT]; int64_t ha_at,climate_at;
    bool ha_ok,climate_ok,has_temp,has_target,simulation,away,live_active,settings_open,camera_fill;
    unsigned pending;int64_t notice_until;
    float temp,target; char mode[32],reason[80],notice[128],ha_error[128];
    unsigned page,generation;
    float probe_f;int64_t probe_at;
} model_t;
typedef struct { int item,action; float target; int64_t at; } command_t;
static model_t model;
static SemaphoreHandle_t mutex;
static QueueHandle_t commands;
static lv_obj_t *body[5],*nav[5],*connection,*notice,*temp_label,*target_label,*mode_label,*reason_label;
static lv_obj_t *states[COUNT],*control[COUNT][6],*minus_button,*plus_button,*away_button,*apply_button,*camera_image,*camera_note,*settings_button;
static lv_obj_t *settings_modal,*settings_volume,*settings_volume_label,*settings_camera_button,*settings_close_button;
static lv_obj_t *temperature_ring,*camera_frame,*camera_status,*tile[COUNT],*icon_badge[COUNT];
static lv_obj_t *bulb_head[COUNT],*bulb_stem[COUNT],*bulb_base[COUNT],*activity_ring[COUNT];
static lv_obj_t *notify_button[4],*probe_test_label;
static lv_obj_t *health_button,*health_dot,*diagnostic_modal,*diagnostic_close_button;
static lv_obj_t *diagnostic_cells[7],*diagnostic_badges[7],*diagnostic_dots[7];
static lv_obj_t *web_access_label,*ship_title,*ship_detail,*ship_note,*ship_map,*ship_markers[ROOM_SHIP_LIMIT],*cottage_marker;
static lv_obj_t *ship_map_note;
static lv_image_dsc_t ship_map_desc;
static void *ship_map_pixels;
static double ship_bounds[4];
static unsigned selected_ship;
static room_ships_t ship_view;
static atomic_uint frame_last_us,frame_max_us,frame_slow,lvgl_free,lvgl_largest,lvgl_stack_free;
static int64_t frame_started;
static lv_obj_t *blind_slats[5],*blind_rail,*heater_fins[4],*heater_rays[3];
static lv_image_dsc_t image_desc;
static void *image_pixels;
static float draft=65;
static bool editing,ring_rendering;
static int64_t last_touch;
static unsigned visible_page;

/* Copy directly into caller storage: returning this large model by value
 * created nested stack temporaries inside LVGL callbacks. */
static void copy_model(model_t *out) {xSemaphoreTake(mutex,portMAX_DELAY);*out=model;xSemaphoreGive(mutex);}
void room_display_probe_sample(float f)
{
    if(!mutex||!isfinite(f))return;
    xSemaphoreTake(mutex,portMAX_DELAY);model.probe_f=f;model.probe_at=esp_timer_get_time();xSemaphoreGive(mutex);
}
static void message(const char *text) { xSemaphoreTake(mutex,portMAX_DELAY);snprintf(model.notice,sizeof(model.notice),"%s",text);model.notice_until=esp_timer_get_time()+7000000;xSemaphoreGive(mutex); }
static const char *str(cJSON *j,const char *key) { cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:""; }
static bool number(cJSON *j,const char *key,float *out) { cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;*out=v->valuedouble;return true; }
static bool yes(cJSON *j,const char *key) { return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,key)); }
static cJSON *json_request(bool ha,const char *path,const char *post,int *code)
{
    if(ha){
        if(!post){*code=200;return room_ha_status();}
        cJSON *payload=cJSON_Parse(post);cJSON *result=room_ha_command(payload,code);cJSON_Delete(payload);if(*code!=200){cJSON_Delete(result);return NULL;}return result;
    }
    if(post){
        cJSON *p=cJSON_Parse(post),*away=cJSON_GetObjectItemCaseSensitive(p,"away");
        if(cJSON_IsBool(away)){
            bool enabled=cJSON_IsTrue(away);
            cJSON_Delete(p);
            return room_controller_away(enabled,code);
        }
        float target=0;bool valid=number(p,"target_f",&target);cJSON_Delete(p);
        if(!valid){*code=400;return NULL;}return room_controller_target(target,code);
    }
    cJSON *j=room_controller_status();*code=j?200:503;return j;
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
            cJSON *sample=cJSON_GetObjectItemCaseSensitive(entry,"sample_us");if(cJSON_IsNumber(sample)&&sample->valuedouble>=0&&sample->valuedouble<9007199254740992.0)i->sampled=(int64_t)sample->valuedouble;
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
    for(int k=0;k<COUNT;++k)room_optimistic_update(&model.optimistic[k],items[k].state,items[k].sampled,esp_timer_get_time());
    memcpy(model.items,items,sizeof(items));model.ha_ok=valid&&yes(j,"connected");model.ha_at=valid?esp_timer_get_time():0;
    xSemaphoreGive(mutex);cJSON_Delete(j);
}
static void poll_climate(void)
{
    int code;cJSON *j=json_request(false,"/api/status",NULL,&code);float t=0,target=0;
    /* Preserve the last acknowledged snapshot through a missed poll. Its
     * original timestamp still expires at FRESH_US; never renew it on failure.
     * room_controller independently requires a fresh lease before commands. */
    if(!j)return;
    bool ht=number(j,"temperature_f",&t),hs=number(j,"target_f",&target)&&target>=45&&target<=80;
    xSemaphoreTake(mutex,portMAX_DELAY);
    model.climate_ok=j!=NULL;model.climate_at=j?esp_timer_get_time():0;
    model.has_temp=ht;model.temp=t;model.has_target=hs;model.target=target;
    model.simulation=yes(j,"simulation");model.away=yes(j,"away");model.live_active=yes(j,"live_active");
    snprintf(model.mode,sizeof(model.mode),"%s",str(j,"mode"));
    snprintf(model.reason,sizeof(model.reason),"%s",str(j,"reason"));
    xSemaphoreGive(mutex);cJSON_Delete(j);
}
static bool current(int64_t at) { return at>0 && esp_timer_get_time()-at<FRESH_US; }
static void execute(command_t c)
{
    model_t m;copy_model(&m);bool allowed=esp_timer_get_time()-c.at<COMMAND_TTL_US;
    if(c.item==-1)allowed=allowed&&m.climate_ok&&m.has_target&&current(m.climate_at)&&!m.away&&c.target>=45&&c.target<=80;
    else if(c.item==-2)allowed=allowed&&m.climate_ok&&m.has_target&&current(m.climate_at);
    else allowed=allowed&&room_wifi_connected()&&current(m.ha_at)&&m.items[c.item].available&&(m.items[c.item].actions&(1U<<c.action));
    if(!allowed)message("Request expired or device unavailable; nothing sent");
    else {
        char payload[200];int code;
        if(c.item==-1)snprintf(payload,sizeof(payload),"{\"target_f\":%.1f}",c.target);
        else if(c.item==-2)snprintf(payload,sizeof(payload),"{\"away\":%s}",c.action?"true":"false");
        else snprintf(payload,sizeof(payload),"{\"key\":\"%s\",\"action\":\"%s\",\"request_id\":\"esp-%08" PRIx32 "-%08" PRIx32 "-%" PRId64 "\"}",keys[c.item],actions[c.action],esp_random(),esp_random(),c.at);
        cJSON *j=json_request(c.item>=0,c.item<0?"/api/config":"/api/ha/command",payload,&code);
        if(code>=400&&c.item>=0){xSemaphoreTake(mutex,portMAX_DELAY);model.optimistic[c.item].active=false;xSemaphoreGive(mutex);}
        if(code>=400){
            /* Show the controller's explicit refusal, not an unhelpful
             * generic toast. We do not re-send ambiguous mutations. */
            const char *detail=j?str(j,"error"):"";
            message(*detail?detail:code==503?
                "Controller is busy or offline. Check its status.":
                "Controller refused this command. Check its mode.");
        }
        else if(!j)message("Connection lost. Verify the target before trying again.");
        else if(c.item>=0&&!yes(j,"accepted"))message("No confirmation. Check current state.");
        else if(c.item==-2)message(c.action?"Away / Freeze Hold 50 F saved":"Home comfort mode restored");
        else if(c.item==-1)message("Target saved");
        cJSON_Delete(j);
        if(c.item<0)poll_climate();else poll_ha();
    }
    xSemaphoreTake(mutex,portMAX_DELAY);model.pending&=~(c.item<0?CLIMATE_PENDING:1U<<c.item);xSemaphoreGive(mutex);
}
/* Polling and command dispatch have separate tasks. An offline controller
 * must not block Home taps or stop local HA status refreshes. */
static void command_task(void *arg)
{
    command_t c;
    for(;;)if(xQueueReceive(commands,&c,portMAX_DELAY)==pdTRUE)execute(c);
}
static void network_task(void *arg)
{
    int64_t ha_due=0,climate_due=0;
    for(;;){
        int64_t now=esp_timer_get_time();
        if(now>=ha_due){poll_ha();ha_due=esp_timer_get_time()+1000000;}
        if(now>=climate_due){poll_climate();climate_due=esp_timer_get_time()+2000000;}
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
static bool camera_current(unsigned generation)
{
    xSemaphoreTake(mutex,portMAX_DELAY);bool showing=!model.settings_open&&model.page==3&&model.generation==generation&&current(model.ha_at)&&model.items[10].available;xSemaphoreGive(mutex);
    return showing&&!room_ota_busy()&&room_wifi_connected();
}
/* Caller holds the display lock; ownership of decoded PSRAM pixels transfers here. */
static void camera_present(void *pixels,unsigned width,unsigned height,unsigned bytes,bool fill)
{
    lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
    lv_image_cache_drop(&image_desc);
    free(image_pixels);image_pixels=pixels;
    image_desc=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
        .w=width,.h=height,.stride=width*2},.data_size=bytes,.data=image_pixels};
    lv_image_set_src(camera_image,&image_desc);
    lv_image_set_scale(camera_image,room_camera_scale(width,height,454,306,fill));
    lv_obj_align(camera_image,LV_ALIGN_CENTER,0,0);
    lv_obj_remove_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
}
static void camera_task(void *arg)
{
    uint8_t *jpeg=heap_caps_malloc(ROOM_HA_CAMERA_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!jpeg){message("Camera memory unavailable");vTaskDelete(NULL);return;}
    bool reported=false;int64_t last_log=0;
    for(;;) {
        model_t m;copy_model(&m);
        if(camera_current(m.generation)) {
            int code=0,n=0;char detail[112]="";
            esp_err_t err=room_ha_camera(jpeg,ROOM_HA_CAMERA_CAPACITY,&code,&n);
            esp_jpeg_image_output_t info={0};void *pixels=NULL;room_camera_size_t size={0};
            if(err!=ESP_OK||code!=200){
                if(code==401||code==403)snprintf(detail,sizeof(detail),"Camera access denied (HTTP %d)",code);
                else if(code==404)snprintf(detail,sizeof(detail),"Camera entity was not found");
                else if(code>=400)snprintf(detail,sizeof(detail),"Home Assistant camera error (HTTP %d)",code);
                else snprintf(detail,sizeof(detail),"Camera request failed; retrying");
            } else {
                room_jpeg_result_t format=room_camera_size(jpeg,(size_t)n,&size);
                if(format==ROOM_JPEG_UNSUPPORTED)snprintf(detail,sizeof(detail),"Camera needs a baseline JPEG snapshot");
                else if(format==ROOM_JPEG_TOO_LARGE)snprintf(detail,sizeof(detail),"Camera snapshot resolution is too large");
                else if(format!=ROOM_JPEG_OK)snprintf(detail,sizeof(detail),"Camera did not return a valid JPEG snapshot");
                else {
                    pixels=heap_caps_malloc(size.bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
                    esp_jpeg_image_cfg_t cfg={.indata=jpeg,.indata_size=n,.out_format=JPEG_IMAGE_FORMAT_RGB565,
                        .out_scale=(esp_jpeg_image_scale_t)size.scale,.outbuf=pixels,.outbuf_size=size.bytes};
                    if(!pixels)snprintf(detail,sizeof(detail),"Camera image memory unavailable");
                    else if(esp_jpeg_decode(&cfg,&info)!=ESP_OK){
                        free(pixels);pixels=NULL;snprintf(detail,sizeof(detail),"Camera JPEG could not be decoded");
                    }
                }
            }
            if(!pixels){
                int64_t now=esp_timer_get_time();
                if(!last_log||now-last_log>15000000){
                    ESP_LOGW(TAG,"Camera: %s; HTTP=%d bytes=%d source=%ux%u err=0x%x",detail,code,n,size.source_width,size.source_height,(unsigned)err);
                    last_log=now;
                }
                reported=false;
            } else if(!reported){
                ESP_LOGI(TAG,"Camera ready: %ux%u -> %ux%u; JPEG bytes=%d",size.source_width,size.source_height,info.width,info.height,n);
                reported=true;
            }
            if(bsp_display_lock(1000)) {
                if(camera_current(m.generation)) {
                    lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
                    if(pixels) {
                        camera_present(pixels,info.width,info.height,info.output_len,m.camera_fill);
                        pixels=NULL;
                        lv_label_set_text(camera_note,"Auto-refreshing snapshots");
                        lv_label_set_text(camera_status,LV_SYMBOL_VIDEO "  LIVE VIEW");
                    } else {
                        lv_label_set_text(camera_note,detail);
                        lv_label_set_text(camera_status,LV_SYMBOL_REFRESH "  RETRYING");
                    }
                }
                bsp_display_unlock();
            }
            free(pixels);
        } else reported=false;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
static void enabled(lv_obj_t *b,bool value) { if(value)lv_obj_remove_state(b,LV_STATE_DISABLED);else lv_obj_add_state(b,LV_STATE_DISABLED); }
static void select_page(unsigned p)
{
    visible_page=p;last_touch=esp_timer_get_time();room_ships_active(p==4);
    if(settings_modal)lv_obj_add_flag(settings_modal,LV_OBJ_FLAG_HIDDEN);
    if(diagnostic_modal)lv_obj_add_flag(diagnostic_modal,LV_OBJ_FLAG_HIDDEN);
    for(unsigned n=0;n<5;++n){if(n==p)lv_obj_remove_flag(body[n],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(body[n],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(nav[n],lv_color_hex(n==p?0x64dfc4:0x151f2d),0);lv_obj_set_style_text_color(nav[n],lv_color_hex(n==p?0x0b111b:0x92a3b8),0);}
    xSemaphoreTake(mutex,portMAX_DELAY);model.page=p;model.settings_open=false;++model.generation;xSemaphoreGive(mutex);
    lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(camera_note,"Loading camera...");
    lv_label_set_text(camera_status,LV_SYMBOL_VIDEO "  CONNECTING");
}
static void open_settings(void);
static void settings_event(lv_event_t *e){(void)e;open_settings();}
static void sound_test_event(lv_event_t *e)
{
    last_touch=esp_timer_get_time();
    if(!room_audio_test())message("Sound test busy; try again");
}
static void notify_event(lv_event_t *e)
{
    int key=(int)(intptr_t)lv_event_get_user_data(e);last_touch=esp_timer_get_time();
    if(!room_audio_set_notify(key,!room_audio_notify_enabled(key)))message("Notification setting busy; try again");
}
static void nav_event(lv_event_t *e) {select_page((unsigned)(uintptr_t)lv_event_get_user_data(e));}
static void gesture(lv_event_t *e)
{
    xSemaphoreTake(mutex,portMAX_DELAY);bool settings_open=model.settings_open;xSemaphoreGive(mutex);
    if(settings_open)return;
    lv_indev_t *input=lv_indev_active();if(!input)return;
    lv_dir_t d=lv_indev_get_gesture_dir(input);
    if(d==LV_DIR_LEFT||d==LV_DIR_RIGHT){select_page((visible_page+(d==LV_DIR_LEFT?1:4))%5);lv_indev_wait_release(input);}
}
bool room_display_request_target(float target)
{
    if(!mutex||!commands||!isfinite(target)||target<45||target>80)return false;
    command_t c={.item=-1,.target=target,.at=esp_timer_get_time()};
    xSemaphoreTake(mutex,portMAX_DELAY);
    bool ready=model.climate_ok&&model.has_target&&!model.away&&current(model.climate_at)&&!(model.pending&CLIMATE_PENDING);
    if(ready){model.pending|=CLIMATE_PENDING;if(xQueueSend(commands,&c,0)!=pdTRUE){model.pending&=~CLIMATE_PENDING;ready=false;}}
    xSemaphoreGive(mutex);return ready;
}
bool room_display_request_away(bool enabled)
{
    if(!mutex||!commands)return false;
    command_t c={.item=-2,.action=enabled?1:0,.at=esp_timer_get_time()};
    xSemaphoreTake(mutex,portMAX_DELAY);
    bool ready=model.climate_ok&&model.has_target&&current(model.climate_at)&&
        !(model.pending&CLIMATE_PENDING);
    if(ready && model.away==enabled){xSemaphoreGive(mutex);return true;}
    if(ready){
        model.pending|=CLIMATE_PENDING;
        if(xQueueSend(commands,&c,0)!=pdTRUE){
            model.pending&=~CLIMATE_PENDING;ready=false;
        }
    }
    xSemaphoreGive(mutex);return ready;
}
/* The Home Assistant action queue is shared by conventional controls and
 * the compact tap-to-toggle light tiles. Never queue a guessed action when
 * HA state is stale, a command is pending, or an action is unavailable. */
static void queue_ha_action(int key,int action,const model_t *snapshot)
{
    if(key<0||key>=COUNT||action<0||action>=6)return;
    const unsigned bit=1U<<key;
    if((snapshot->pending&bit)||!room_wifi_connected()||
       !current(snapshot->ha_at)||!snapshot->items[key].available||
       !(snapshot->items[key].actions&(1U<<action)))return;
    command_t c={.item=key,.action=action,.at=esp_timer_get_time()};
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(model.pending&bit){
        xSemaphoreGive(mutex);
        return;
    }
    model.pending|=bit;
    if((key==0||key==1||key==7)&&(action==0||action==1))
        room_optimistic_begin(&model.optimistic[key],action==0,c.at);
    xSemaphoreGive(mutex);
    if(xQueueSend(commands,&c,0)!=pdTRUE){
        xSemaphoreTake(mutex,portMAX_DELAY);
        model.pending&=~bit;
        model.optimistic[key].active=false;
        xSemaphoreGive(mutex);
        message("Controller busy. Light command was not queued.");
    }
}
static void light_tile_event(lv_event_t *e)
{
    const int key=(int)(intptr_t)lv_event_get_user_data(e);
    if(key!=0&&key!=1)return;
    static model_t snapshot;
    copy_model(&snapshot);
    last_touch=esp_timer_get_time();
    if(snapshot.pending&(1U<<key))return;
    /* On and Off are the only known toggle states. Unknown/unavailable
     * state intentionally does nothing; the next refresh will restore it. */
    int action=!strcmp(snapshot.items[key].state,"on")?1:
               !strcmp(snapshot.items[key].state,"off")?0:-1;
    if(action>=0)queue_ha_action(key,action,&snapshot);
}
static void send_event(lv_event_t *e)
{
    intptr_t id=(intptr_t)lv_event_get_user_data(e);
    static model_t snapshot;
    copy_model(&snapshot);
    last_touch=esp_timer_get_time();
    if(id==-10){
        if(room_display_request_away(!snapshot.away))editing=false;
        return;
    }
    if(id==-2||id==-3){
        if(!(snapshot.pending&CLIMATE_PENDING)&&snapshot.climate_ok&&
           snapshot.has_target&&!snapshot.away&&current(snapshot.climate_at)){
            draft=fmaxf(45,fminf(80,draft+(id==-2?-1.0f:1.0f)));
            editing=true;
        }
        return;
    }
    if(id<0){
        if(room_display_request_target(draft))editing=false;
        return;
    }
    queue_ha_action((int)id/6,(int)id%6,&snapshot);
}
/* The existing arc now controls a DRAFT target instead of pretending the
 * ambient room temperature is the adjustable setting. Finger movement only
 * updates the local preview; one explicit Set Temperature commits once. */
static void target_ring_event(lv_event_t *e)
{
    (void)e;
    if(ring_rendering)return; /* Painting status must never edit a setpoint. */
    /* Large model stays off the LVGL callback stack. */
    static model_t snapshot;copy_model(&snapshot);
    last_touch=esp_timer_get_time();
    if(!snapshot.climate_ok || !snapshot.has_target || snapshot.away ||
       !current(snapshot.climate_at) || (snapshot.pending&CLIMATE_PENDING))
        return;
    const int selected=(int)lv_arc_get_value(temperature_ring);
    draft=fmaxf(45.0f,fminf(80.0f,(float)selected));
    editing=true;
}
static const char *climate_reason_label(const char *reason)
{
    if(!reason || !*reason)return "Waiting for furnace status";
    if(!strcmp(reason,"mapped_setting_reached"))return "Dial at requested setting";
    if(!strcmp(reason,"bench_move"))return "Adjusting furnace dial";
    if(!strcmp(reason,"overshoot_trim"))return "Reducing heat after overshoot";
    if(!strcmp(reason,"setpoint_cooldown"))return "Waiting briefly before adjustment";
    if(!strcmp(reason,"movement_dwell"))return "Waiting between adjustments";
    if(!strcmp(reason,"temperature_stale"))return "Waiting for fresh room temperature";
    if(!strcmp(reason,"calibration_required"))return "Furnace calibration required";
    if(!strcmp(reason,"sensor_disagreement"))return "Temperature sensors disagree";
    if(!strcmp(reason,"target_outside_measured_map"))return "Setpoint outside calibrated range";
    if(!strcmp(reason,"position_unsynchronized"))return "Furnace position not synchronized";
    if(!strcmp(reason,"verifying_encoder"))return "Verifying dial position";
    if(!strcmp(reason,"hold"))return "Automatic heating control paused";
    if(!strcmp(reason,"pwm_error"))return "Servo output fault - Service required";
    return "Controller status available in diagnostics";
}
/* Native styling uses LVGL primitives, so no browser or bitmap UI is required. */
#define INK 0x0b111b
#define CARD 0x151f2d
#define EDGE 0x263449
#define MUTED 0x92a3b8
#define TEXT 0xf1f5fa
#define MINT 0x64dfc4
#define AMBER 0xf5b65d
#define BLUE 0x7eb5ff
static lv_obj_t *label(lv_obj_t *parent,const char *text,int x,int y,int width,const lv_font_t *font)
{
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_obj_set_style_text_font(o,font,0);lv_label_set_text(o,text);
    lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);return o;
}
static lv_obj_t *panel(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(parent);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,radius,0);lv_obj_set_style_border_color(o,lv_color_hex(EDGE),0);
    lv_obj_set_style_border_width(o,1,0);lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);return o;
}
static void centered(lv_obj_t *o){lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);}
static void tint(lv_obj_t *o,uint32_t color){lv_obj_set_style_text_color(o,lv_color_hex(color),0);}
static lv_obj_t *button(lv_obj_t *parent,const char *text,int x,int y,int width,int height,lv_event_cb_t cb,intptr_t id)
{
    lv_obj_t *o=lv_button_create(parent);lv_obj_remove_style_all(o);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,width,height);
    lv_obj_set_style_bg_color(o,lv_color_hex(EDGE),0);tint(o,TEXT);
    lv_obj_set_style_radius(o,12,0);lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0x334861),LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(o,LV_OPA_40,LV_STATE_DISABLED);lv_obj_set_style_text_opa(o,LV_OPA_40,LV_STATE_DISABLED);
    lv_obj_add_flag(o,LV_OBJ_FLAG_GESTURE_BUBBLE);lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,(void*)id);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,text);lv_obj_center(l);return o;
}
static void settings_close_event(lv_event_t *e)
{
    (void)e;last_touch=esp_timer_get_time();lv_obj_add_flag(settings_modal,LV_OBJ_FLAG_HIDDEN);
    xSemaphoreTake(mutex,portMAX_DELAY);model.settings_open=false;++model.generation;xSemaphoreGive(mutex);
}
static void settings_volume_event(lv_event_t *e)
{
    last_touch=esp_timer_get_time();unsigned volume=(unsigned)lv_slider_get_value(settings_volume);
    lv_label_set_text_fmt(settings_volume_label,"Volume: %u%%%s",volume,volume?"":" (muted)");
    if(lv_event_get_code(e)==LV_EVENT_RELEASED&&!room_audio_set_volume(volume))message("Volume setting busy; try again");
}
static void settings_camera_event(lv_event_t *e)
{
    (void)e;last_touch=esp_timer_get_time();
    xSemaphoreTake(mutex,portMAX_DELAY);model.camera_fill=!model.camera_fill;++model.generation;bool fill=model.camera_fill;xSemaphoreGive(mutex);
    lv_label_set_text(lv_obj_get_child(settings_camera_button,0),fill?"Camera: Fill (crop edges)":"Camera: Fit (whole image)");
}
static void open_settings(void)
{
    last_touch=esp_timer_get_time();
    if(diagnostic_modal)lv_obj_add_flag(diagnostic_modal,LV_OBJ_FLAG_HIDDEN);
    if(!settings_modal){
        settings_modal=panel(lv_screen_active(),12,57,456,364,CARD,18);
        lv_obj_add_flag(settings_modal,LV_OBJ_FLAG_CLICKABLE);
        label(settings_modal,"Settings",20,16,240,&lv_font_montserrat_24);
        settings_close_button=button(settings_modal,"Close",342,12,94,34,settings_close_event,0);
        settings_volume_label=label(settings_modal,"",24,65,400,&lv_font_montserrat_20);
        settings_volume=lv_slider_create(settings_modal);lv_obj_set_pos(settings_volume,36,111);lv_obj_set_size(settings_volume,384,16);
        lv_slider_set_range(settings_volume,0,100);
        lv_obj_set_style_bg_color(settings_volume,lv_color_hex(MINT),LV_PART_INDICATOR);
        lv_obj_add_event_cb(settings_volume,settings_volume_event,LV_EVENT_VALUE_CHANGED,NULL);
        lv_obj_add_event_cb(settings_volume,settings_volume_event,LV_EVENT_RELEASED,NULL);
        button(settings_modal,LV_SYMBOL_PLAY "  Test sound",24,150,408,42,sound_test_event,0);
        settings_camera_button=button(settings_modal,"",24,202,408,40,settings_camera_event,0);
        web_access_label=label(settings_modal,"",24,254,408,&lv_font_montserrat_14);
        if(CONFIG_ROOM_SENSOR==3)probe_test_label=label(settings_modal,"BMP/BME280 test: waiting for sensor",24,324,408,&lv_font_montserrat_14);
    }
    char address[64];room_wifi_address(address,sizeof(address));
    lv_label_set_text_fmt(web_access_label,"Web: http://%s/settings",address);
    lv_slider_set_value(settings_volume,(int32_t)room_audio_get_volume(),LV_ANIM_OFF);
    lv_label_set_text_fmt(settings_volume_label,"Volume: %u%%%s",room_audio_get_volume(),room_audio_get_volume()?"":" (muted)");
    xSemaphoreTake(mutex,portMAX_DELAY);model.settings_open=true;++model.generation;bool fill=model.camera_fill;xSemaphoreGive(mutex);
    lv_label_set_text(lv_obj_get_child(settings_camera_button,0),fill?"Camera: Fill (crop edges)":"Camera: Fit (whole image)");
    lv_obj_remove_flag(settings_modal,LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(settings_modal,-1);
}
static void diagnostics_close(lv_event_t *e)
{
    (void)e;last_touch=esp_timer_get_time();lv_obj_add_flag(diagnostic_modal,LV_OBJ_FLAG_HIDDEN);
    xSemaphoreTake(mutex,portMAX_DELAY);model.settings_open=false;++model.generation;xSemaphoreGive(mutex);
}
static void diagnostics_event(lv_event_t *e)
{
    (void)e;last_touch=esp_timer_get_time();
    if(settings_modal)lv_obj_add_flag(settings_modal,LV_OBJ_FLAG_HIDDEN);
    if(!diagnostic_modal){
        diagnostic_modal=panel(lv_screen_active(),12,57,456,364,CARD,18);
        lv_obj_add_flag(diagnostic_modal,LV_OBJ_FLAG_CLICKABLE);
        label(diagnostic_modal,"Connections",20,16,290,&lv_font_montserrat_24);
        diagnostic_close_button=button(diagnostic_modal,"Close",342,12,94,34,diagnostics_close,0);
        label(diagnostic_modal,"SERVICE",24,60,176,&lv_font_montserrat_14);
        label(diagnostic_modal,"STATUS",224,60,208,&lv_font_montserrat_14);
        const char *names[]={"Wi-Fi","Home Assistant","Heat controller","ESP-NOW","Local sensor","Ship tracking","MQTT thermostat"};
        for(unsigned i=0;i<7;++i){
            label(diagnostic_modal,names[i],24,91+i*31,190,&lv_font_montserrat_16);
            diagnostic_badges[i]=panel(diagnostic_modal,220,86+i*31,212,28,CARD,8);
            diagnostic_dots[i]=panel(diagnostic_badges[i],9,10,8,8,MUTED,4);lv_obj_set_style_border_width(diagnostic_dots[i],0,0);
            diagnostic_cells[i]=label(diagnostic_badges[i],"",26,6,180,&lv_font_montserrat_14);
        }
        label(diagnostic_modal,"Green: connected   Yellow: not configured",24,316,408,&lv_font_montserrat_14);
        label(diagnostic_modal,"Red: offline / stale   Orange: ESP-NOW",24,337,408,&lv_font_montserrat_14);
    }
    xSemaphoreTake(mutex,portMAX_DELAY);model.settings_open=true;++model.generation;xSemaphoreGive(mutex);
    lv_obj_remove_flag(diagnostic_modal,LV_OBJ_FLAG_HIDDEN);lv_obj_move_to_index(diagnostic_modal,-1);
}
/* Small semantic icons drawn in LVGL, with no external fonts or image buffers. */
static lv_obj_t *icon_bar(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=panel(p,x,y,w,h,color,radius);lv_obj_set_style_border_width(o,0,0);return o;
}
static void blinds_icon(lv_obj_t *parent)
{
    lv_obj_t *p=panel(parent,9,9,34,49,CARD,0);
    lv_obj_set_style_bg_opa(p,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(p,0,0);
    icon_bar(p,0,0,31,5,BLUE,2);
    icon_bar(p,0,6,2,39,0x40556e,0);icon_bar(p,29,6,2,39,0x40556e,0);
    for(unsigned n=0;n<5;++n)blind_slats[n]=icon_bar(p,3,8+n*7,25,3,BLUE,1);
    blind_rail=icon_bar(p,2,39,27,4,BLUE,1);
    icon_bar(p,33,5,1,26,MUTED,0);icon_bar(p,31,30,4,5,MUTED,2);
}
static void heater_icon(lv_obj_t *parent)
{
    lv_obj_t *p=panel(parent,8,8,36,50,CARD,0);
    lv_obj_set_style_bg_opa(p,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(p,0,0);
    icon_bar(p,2,19,31,3,0x647386,1);icon_bar(p,2,35,31,3,0x647386,1);
    for(unsigned n=0;n<4;++n)heater_fins[n]=icon_bar(p,2+n*8,14,6,28,0x647386,3);
    icon_bar(p,4,40,4,6,0x647386,1);icon_bar(p,28,40,4,6,0x647386,1);
    for(unsigned n=0;n<3;++n){heater_rays[n]=icon_bar(p,7+n*10,2+(n==1?0:3),2,7,AMBER,1);lv_obj_add_flag(heater_rays[n],LV_OBJ_FLAG_HIDDEN);}
}
static void ship_select(lv_event_t *e){selected_ship=(unsigned)(uintptr_t)lv_event_get_user_data(e);last_touch=esp_timer_get_time();}
static void ship_change(lv_event_t *e){
    int delta=(int)(intptr_t)lv_event_get_user_data(e);last_touch=esp_timer_get_time();
    if(ship_view.count)selected_ship=(selected_ship+ship_view.count+delta)%ship_view.count;
}
static void refresh_ships(void)
{
    static int64_t last_snapshot=-1,last_age=-1;static unsigned last_selected=UINT_MAX,last_count=UINT_MAX,last_failures=UINT_MAX;
    static bool last_service,last_feed;
    double bounds[4];void *pixels=room_ships_take_map(bounds);
    if(pixels){
        lv_obj_add_flag(ship_map,LV_OBJ_FLAG_HIDDEN);lv_image_cache_drop(&ship_map_desc);free(ship_map_pixels);ship_map_pixels=pixels;
        memcpy(ship_bounds,bounds,sizeof(ship_bounds));
        ship_map_desc=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=456,.h=400,.stride=912},.data_size=456*400*2,.data=pixels};
        lv_image_set_src(ship_map,&ship_map_desc);lv_image_set_scale(ship_map,154);lv_obj_align(ship_map,LV_ALIGN_CENTER,0,0);
        lv_obj_remove_flag(ship_map,LV_OBJ_FLAG_HIDDEN);
    }
    int64_t age=esp_timer_get_time()/30000000;
    if(!pixels&&last_snapshot==ship_view.received_at&&last_selected==selected_ship&&last_count==ship_view.count&&last_failures==ship_view.failures&&last_age==age&&last_service==ship_view.service&&last_feed==ship_view.feed)return;
    last_snapshot=ship_view.received_at;last_selected=selected_ship;last_count=ship_view.count;last_failures=ship_view.failures;last_age=age;last_service=ship_view.service;last_feed=ship_view.feed;
    bool fresh=ship_view.service;unsigned count=fresh?ship_view.count:0;
    if(selected_ship>=count)selected_ship=0;
    lv_label_set_text_fmt(ship_title,"Ships nearby  /  %u mi",room_settings_ship_range());
    if(!room_settings_ships_enabled())lv_label_set_text(ship_detail,"Enable ship tracking in web settings");
    else if(!fresh)lv_label_set_text(ship_detail,"Ship service offline\n\nRetrying automatically");
    else if(!count)lv_label_set_text(ship_detail,"No ships within this range");
    else{
        room_ship_t *v=&ship_view.ships[selected_ship];char speed[32];
        if(v->speed_valid)snprintf(speed,sizeof(speed),"%.1f kn",v->speed);else snprintf(speed,sizeof(speed),"Speed unavailable");
        unsigned age=v->age+(esp_timer_get_time()-ship_view.received_at)/1000000;
        char detail[384];snprintf(detail,sizeof(detail),"%s\n\n%.1f mi / %s\n\n%s\n\n%s\n\n%s / %u min old",v->name,v->distance,speed,v->kind,v->destination,age>600?"STALE":"AIS",age/60);
        lv_label_set_text(ship_detail,detail);
    }
    lv_label_set_text_fmt(ship_note,!fresh?"Waiting for ship service":!ship_view.feed?"AIS feed offline; cached positions shown":"%u nearby / %u shown / refresh 10 s",ship_view.nearby,count);
    bool map_ready=ship_map_pixels!=NULL;
    if(map_ready)lv_obj_add_flag(ship_map_note,LV_OBJ_FLAG_HIDDEN);
    else lv_label_set_text(ship_map_note,fresh?"Map loading...\nRetrying automatically":"Waiting for ship service");
    if(map_ready){
        float x=(ship_view.longitude-ship_bounds[0])/(ship_bounds[2]-ship_bounds[0])*274;
        float y=(ship_bounds[3]-ship_view.latitude)/(ship_bounds[3]-ship_bounds[1])*240;
        lv_obj_set_pos(cottage_marker,(int)x-8,(int)y-8);lv_obj_remove_flag(cottage_marker,LV_OBJ_FLAG_HIDDEN);
    }
    for(unsigned i=0;i<ROOM_SHIP_LIMIT;++i){
        if(map_ready&&i<count){room_ship_t *v=&ship_view.ships[i];
            float x=(v->lon-ship_bounds[0])/(ship_bounds[2]-ship_bounds[0])*274,y=(ship_bounds[3]-v->lat)/(ship_bounds[3]-ship_bounds[1])*240;
            if(x<0||x>274||y<0||y>240){lv_obj_add_flag(ship_markers[i],LV_OBJ_FLAG_HIDDEN);continue;}
            lv_obj_set_pos(ship_markers[i],(int)x-11,(int)y-11);
            lv_obj_set_style_bg_color(ship_markers[i],lv_color_hex(i==selected_ship?MINT:AMBER),0);tint(ship_markers[i],INK);
            lv_obj_remove_flag(ship_markers[i],LV_OBJ_FLAG_HIDDEN);
        }else lv_obj_add_flag(ship_markers[i],LV_OBJ_FLAG_HIDDEN);
    }
}
static void frame_event(lv_event_t *e)
{
    if(lv_event_get_code(e)==LV_EVENT_REFR_START)frame_started=esp_timer_get_time();
    else if(frame_started){unsigned elapsed=(unsigned)(esp_timer_get_time()-frame_started);frame_last_us=elapsed;
        if(elapsed>frame_max_us)frame_max_us=elapsed;
        if(elapsed>50000)++frame_slow;
        frame_started=0;}
}
bool room_display_camera_fill(void){if(!mutex)return false;xSemaphoreTake(mutex,portMAX_DELAY);bool fill=model.camera_fill;xSemaphoreGive(mutex);return fill;}
void room_display_set_camera_fill(bool fill){if(mutex){xSemaphoreTake(mutex,portMAX_DELAY);model.camera_fill=fill;++model.generation;xSemaphoreGive(mutex);}}
/* Shared by the webpage and MQTT diagnostics; never gate on controller transport. */
static void service_words(const model_t *snapshot,const char **words)
{
    bool wifi=room_wifi_connected(),radio=room_controller_fallback();
    bool ha=wifi&&snapshot->ha_ok&&current(snapshot->ha_at),heat=snapshot->climate_ok&&current(snapshot->climate_at);
    bool ha_configured=*room_settings_ha_token()!=0,ship_configured=room_settings_ships_enabled();
    bool ship_service=room_ships_service_online(),ship_online=room_ships_online();
    bool probe=snapshot->probe_at&&esp_timer_get_time()-snapshot->probe_at<30000000;
    int mq=room_mqtt_state();bool heat_configured=room_controller_radio_ready()||*room_settings_controller_url();
    const char *values[]={wifi?"Connected":"Disconnected",!ha_configured?"Not configured":ha?"Online":"Offline",
        !heat_configured?"Not configured":heat?(radio?"ESP-NOW":"Online"):"Offline",!room_controller_radio_ready()?"Not configured":radio&&heat?"Active":"Standby",
        !CONFIG_ROOM_SENSOR?"Not configured":probe?"Fresh":"No fresh sample",!ship_configured?"Not configured":!ship_service?"Service offline":ship_online?"Online":"AIS feed offline",
        mq==2?"Connected":mq==0?"Not configured":"Offline"};
    for(unsigned i=0;i<7;++i)words[i]=values[i];
}
cJSON *room_display_mqtt_state(void)
{
    int64_t now=esp_timer_get_time();float temperature,target;bool sensor,controller,has_target,simulation,away,live_active;char mode[32];model_t snapshot={0};
    xSemaphoreTake(mutex,portMAX_DELAY);
    sensor=model.probe_at&&now-model.probe_at<30000000;temperature=model.probe_f;
    controller=model.climate_ok&&current(model.climate_at);has_target=controller&&model.has_target;target=model.target;simulation=model.simulation;away=model.away;live_active=model.live_active;
    snapshot.ha_ok=model.ha_ok;snapshot.ha_at=model.ha_at;snapshot.climate_ok=controller;snapshot.climate_at=model.climate_at;snapshot.probe_at=model.probe_at;
    snprintf(mode,sizeof(mode),"%s",model.mode);xSemaphoreGive(mutex);
    cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"sensor_fresh",sensor);cJSON_AddBoolToObject(j,"controller_online",controller);
    if(sensor)cJSON_AddNumberToObject(j,"temperature_f",temperature);else cJSON_AddNullToObject(j,"temperature_f");
    if(has_target)cJSON_AddNumberToObject(j,"target_f",target);else cJSON_AddNullToObject(j,"target_f");
    cJSON_AddStringToObject(j,"mode",controller?mode:"unavailable");cJSON_AddBoolToObject(j,"simulation",simulation);
    cJSON_AddBoolToObject(j,"away",controller&&away);
    cJSON_AddBoolToObject(j,"live_active",controller&&live_active);
    const char *words[7],*service_keys[]={"wifi","ha","controller","espnow","sensor","ships","mqtt"};service_words(&snapshot,words);
    cJSON *services=cJSON_AddObjectToObject(j,"services");for(unsigned i=0;i<7;++i)cJSON_AddStringToObject(services,service_keys[i],words[i]);
    cJSON_AddStringToObject(j,"temperature_source",CONFIG_ROOM_SENSOR==3?"BMP/BME280 local test":"Local primary probe");return j;
}
cJSON *room_display_status(void)
{
    model_t m;copy_model(&m);bool wifi=room_wifi_connected(),radio=room_controller_fallback();
    bool ha=wifi&&m.ha_ok&&current(m.ha_at),heat=m.climate_ok&&current(m.climate_at);
    room_health_t health={.wifi=wifi,.ha=ha,.controller=heat,.radio=radio,.ha_configured=*room_settings_ha_token()!=0,
        .ship_configured=room_settings_ships_enabled(),.ship_online=room_ships_online()};
    room_health_status_t h=room_health_status(health);cJSON *j=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"summary",h.text);cJSON_AddStringToObject(j,"summary_kind",h.color==0x64dfc4?"ok":h.color==0xffa24c?"fallback":h.color==0xf5cc65?"unconfigured":"offline");cJSON *a=cJSON_AddArrayToObject(j,"services");
    const char *names[]={"Wi-Fi","Home Assistant","Heat controller","ESP-NOW","Local sensor","Ship tracking","MQTT thermostat"};
    bool probe=m.probe_at&&esp_timer_get_time()-m.probe_at<30000000;
    int mq=room_mqtt_state();
    bool heat_configured=room_controller_radio_ready()||(*room_settings_controller_url()!=0);
    const char *words[7];service_words(&m,words);
    const char *colors[]={wifi?"ok":"offline",!health.ha_configured?"unconfigured":ha?"ok":"offline",
        !heat_configured?"unconfigured":heat?(radio?"fallback":"ok"):"offline",!room_controller_radio_ready()?"unconfigured":radio&&heat?"fallback":"standby",
        !CONFIG_ROOM_SENSOR?"unconfigured":probe?"ok":"offline",!health.ship_configured?"unconfigured":health.ship_online?"ok":"offline",
        mq==2?"ok":mq==0?"unconfigured":"offline"};
    for(unsigned i=0;i<7;++i){cJSON *v=cJSON_CreateObject();cJSON_AddStringToObject(v,"name",names[i]);cJSON_AddStringToObject(v,"state",words[i]);cJSON_AddStringToObject(v,"kind",colors[i]);cJSON_AddItemToArray(a,v);}
    if(probe)cJSON_AddNumberToObject(j,"temperature_f",m.probe_f);
    if(heat&&m.has_target)cJSON_AddNumberToObject(j,"target_f",m.target);
    cJSON_AddBoolToObject(j,"away",heat&&m.away);
    cJSON_AddBoolToObject(j,"live_active",heat&&m.live_active);
    cJSON *perf=cJSON_AddObjectToObject(j,"performance");
    cJSON_AddNumberToObject(perf,"frame_ms",frame_last_us/1000.0);cJSON_AddNumberToObject(perf,"max_frame_ms",frame_max_us/1000.0);
    cJSON_AddNumberToObject(perf,"frames_over_50ms",frame_slow);
    cJSON_AddNumberToObject(perf,"lvgl_stack_min_free",lvgl_stack_free);
    cJSON_AddNumberToObject(perf,"free_lvgl",lvgl_free);cJSON_AddNumberToObject(perf,"largest_lvgl",lvgl_largest);
    cJSON_AddNumberToObject(perf,"free_internal",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(perf,"largest_internal",heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(perf,"free_psram",heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    room_ships_t *ships=malloc(sizeof(*ships));if(ships){room_ships_snapshot(ships);cJSON *p=cJSON_AddObjectToObject(j,"ships");
        cJSON_AddNumberToObject(p,"nearby",ships->nearby);cJSON_AddNumberToObject(p,"count",ships->count);cJSON_AddNumberToObject(p,"request_ms",ships->latency_ms);
        cJSON_AddBoolToObject(p,"service_online",ships->service);cJSON_AddBoolToObject(p,"feed_online",ships->service&&ships->feed);
        cJSON_AddStringToObject(p,"last_error",ships->error);cJSON_AddNumberToObject(p,"http_status",ships->http_status);cJSON_AddNumberToObject(p,"transport_error",ships->transport_error);
        cJSON_AddNumberToObject(p,"bytes",ships->bytes);cJSON_AddNumberToObject(p,"failures",ships->failures);free(ships);}
    cJSON_AddItemToObject(j,"mqtt",room_mqtt_status());
    cJSON_AddStringToObject(j,"integration_sync",room_controller_sync_state());return j;
}
static void refresh(lv_timer_t *timer)
{
    static unsigned monitor_tick;
    if((++monitor_tick%4)==0){lv_mem_monitor_t mem;lv_mem_monitor(&mem);lvgl_free=mem.free_size;lvgl_largest=mem.free_biggest_size;lvgl_stack_free=uxTaskGetStackHighWaterMark(NULL);}
    /* Only LVGL (or locked startup) calls refresh; avoid a multi-KiB task-stack local. */
    static model_t m;copy_model(&m);int64_t now=esp_timer_get_time();
    room_ships_snapshot(&ship_view);if(visible_page==4)refresh_ships();
    if((visible_page||m.settings_open)&&now-last_touch>60000000){select_page(0);copy_model(&m);}
    if(probe_test_label){
        if(m.probe_at&&now-m.probe_at<30000000){
            char reading[64];snprintf(reading,sizeof(reading),"BMP/BME280 test: %.1f F",m.probe_f);
            lv_label_set_text(probe_test_label,reading);
        }
        else lv_label_set_text(probe_test_label,"BMP/BME280 test: no fresh reading");
    }
    bool radio=room_controller_fallback(),wifi=room_wifi_connected();
    bool ha=wifi&&m.ha_ok&&current(m.ha_at),climate=m.climate_ok&&current(m.climate_at);
    room_health_t health={.wifi=wifi,.ha=ha,.controller=climate,.radio=radio,
        .local_sensor=m.probe_at&&now-m.probe_at<30000000,
        .ha_configured=*room_settings_ha_token()!=0,.radio_configured=room_controller_radio_ready(),
        .ship_configured=room_settings_ships_enabled(),.ship_online=room_ships_online()};
    room_health_status_t status=room_health_status(health);
    if(status.other_faults)lv_label_set_text_fmt(connection,"%s +%u",status.text,status.other_faults);
    else lv_label_set_text(connection,status.text);
    tint(connection,status.color);lv_obj_set_style_bg_color(health_dot,lv_color_hex(status.color),0);
    lv_obj_set_style_bg_color(lv_screen_active(),lv_color_hex(!ha&&!climate?0x281218:INK),0);
    bool heat_configured=room_controller_radio_ready()||(*room_settings_controller_url()!=0);
    if(diagnostic_cells[0]&&!lv_obj_has_flag(diagnostic_modal,LV_OBJ_FLAG_HIDDEN)){
        const char *words[7]={wifi?"Connected":"Disconnected",
            !health.ha_configured?"Not configured":ha?"Online":"Offline",
            !heat_configured?"Not configured":climate?(radio?"ESP-NOW":"Online"):"Offline",
            !health.radio_configured?"Not configured":radio&&climate?"Active":room_controller_radio_online()?"Ready":"Standby",
            CONFIG_ROOM_SENSOR==0?"Not configured":health.local_sensor?"Fresh":"No fresh sample",
            !health.ship_configured?"Not configured":health.ship_online?"Online":ship_view.service?"AIS feed offline":"Service offline",
            room_mqtt_state()==2?"Connected":room_mqtt_state()==0?"Not configured":"Offline"};
        unsigned kinds[7]={wifi?0:2,!health.ha_configured?1:ha?0:2,!heat_configured?1:climate?(radio?3:0):2,
            !health.radio_configured?1:radio&&climate?3:room_controller_radio_online()?0:1,
            CONFIG_ROOM_SENSOR==0?1:health.local_sensor?0:2,!health.ship_configured?1:health.ship_online?0:2,
            room_mqtt_state()==2?0:room_mqtt_state()==0?1:2};
        const uint32_t colors[]={0x64dfc4,0xf5cc65,0xff6464,0xffa24c},fills[]={0x153a30,0x3b341c,0x3c202a,0x3c2c1d};
        for(unsigned i=0;i<7;++i){lv_label_set_text(diagnostic_cells[i],words[i]);tint(diagnostic_cells[i],colors[kinds[i]]);
            lv_obj_set_style_bg_color(diagnostic_dots[i],lv_color_hex(colors[kinds[i]]),0);
            lv_obj_set_style_bg_color(diagnostic_badges[i],lv_color_hex(fills[kinds[i]]),0);}
    }
    if(*m.notice&&now<m.notice_until){lv_label_set_text(notice,m.notice);lv_obj_remove_flag(notice,LV_OBJ_FLAG_HIDDEN);}
    else lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
    if(!editing&&climate&&m.has_target)draft=m.target;
    if(m.away)editing=false;
    bool local=CONFIG_ROOM_SENSOR!=0;
    bool room_temp=local?(m.probe_at&&now-m.probe_at<30000000):(climate&&m.has_temp);
    float room_f=local?m.probe_f:m.temp;
    char text[80];
    if(room_temp){snprintf(text,sizeof(text),"%.1f F",room_f);lv_label_set_text(temp_label,text);}
    else lv_label_set_text(temp_label,"-- F");
    if(climate&&m.has_target){
        if(m.away)snprintf(text,sizeof(text),"AWAY / FREEZE  %.1f F",m.target);
        else snprintf(text,sizeof(text),"%s %.1f F",editing?"Set to":"Target",draft);
        lv_label_set_text(target_label,text);
    }
    else lv_label_set_text(target_label,"Target unavailable");
    /* Slider is for the proposed TARGET, not the actual room temperature.
     * No network/servo command is issued until the user presses Set. */
    const int shown_target=(int)lroundf(m.away?50.0f:editing?draft:m.target);
    ring_rendering=true;
    lv_arc_set_value(temperature_ring,climate&&m.has_target?shown_target:45);
    ring_rendering=false;
    lv_obj_set_style_arc_color(temperature_ring,lv_color_hex(m.away?MUTED:AMBER),LV_PART_INDICATOR);
    const bool bench=climate&&!m.simulation&&!m.live_active&&!strcmp(m.mode,"thermostat");
    if(!climate)lv_label_set_text(mode_label,"Controller Offline");
    else if(m.away)lv_label_set_text(mode_label,m.live_active?"Away - Live Heating":
        bench?"Away - Bench Control":"Away - Service");
    else if(m.live_active)lv_label_set_text(mode_label,"Live Heating - Active");
    else if(bench)lv_label_set_text(mode_label,"Bench Control - Active");
    else lv_label_set_text(mode_label,"Service - Standby");
    lv_label_set_text(reason_label,climate?climate_reason_label(m.reason):
        "Climate control waiting for connection");
    const bool can_adjust=climate&&m.has_target&&!m.away&&!(m.pending&CLIMATE_PENDING);
    enabled(minus_button,can_adjust);enabled(plus_button,can_adjust);
    enabled(apply_button,can_adjust&&editing);
    enabled(away_button,climate&&m.has_target&&!(m.pending&CLIMATE_PENDING));
    if(can_adjust)lv_obj_remove_state(temperature_ring,LV_STATE_DISABLED);
    else lv_obj_add_state(temperature_ring,LV_STATE_DISABLED);
    lv_label_set_text(lv_obj_get_child(away_button,0),m.away?"Resume Home":"Away (50 F)");
    lv_obj_set_style_bg_color(away_button,lv_color_hex(m.away?MINT:EDGE),0);
    for(int k=0;k<COUNT;++k){
        bool available=ha&&m.items[k].available;
        bool predicted=available&&room_optimistic_update(&m.optimistic[k],m.items[k].state,m.items[k].sampled,now);
        if(predicted)snprintf(m.items[k].state,sizeof(m.items[k].state),"%s",m.optimistic[k].want_on?"on":"off");
        bool laundry=k>=3&&k<=6;
        room_visual_t visual=available?room_visual_state(m.items[k].state,laundry):ROOM_VIS_UNKNOWN;
        bool active=visual==ROOM_VIS_ACTIVE;
        uint32_t color=active?(laundry?(k<5?BLUE:AMBER):(k==0||k==1?0xffd54a:k==7?AMBER:MINT)):
            visual==ROOM_VIS_DONE?MINT:visual==ROOM_VIS_ERROR?0xff7979:visual==ROOM_VIS_PAUSED?AMBER:MUTED;
        if(k==8&&available)color=TEXT;
        bool pending=(m.pending&(1U<<k))!=0;
        if(k==2&&blind_rail){
            bool closed=!strcmp(m.items[k].state,"closed"),open=!strcmp(m.items[k].state,"open");
            unsigned gap=available?(closed?7:open?2:4):7;
            uint32_t shade=available?(active?MINT:BLUE):MUTED;
            for(unsigned n=0;n<5;++n){lv_obj_set_y(blind_slats[n],8+n*gap);lv_obj_set_style_bg_color(blind_slats[n],lv_color_hex(shade),0);}
            lv_obj_set_y(blind_rail,11+4*gap);lv_obj_set_style_bg_color(blind_rail,lv_color_hex(shade),0);
        }
        if(k==7&&heater_fins[0]){
            for(unsigned n=0;n<4;++n)lv_obj_set_style_bg_color(heater_fins[n],lv_color_hex(available&&active?AMBER:0x647386),0);
            for(unsigned n=0;n<3;++n){if(available&&active)lv_obj_remove_flag(heater_rays[n],LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(heater_rays[n],LV_OBJ_FLAG_HIDDEN);}
        }
        if(states[k]){lv_label_set_text(states[k],(pending&&!predicted)?"Sending...":available?m.items[k].state:k==8?"--":"Unavailable");tint(states[k],pending?AMBER:color);}
        if(icon_badge[k]){
            const char *symbol=k==2?((!strcmp(m.items[k].state,"closed")||!strcmp(m.items[k].state,"closing"))?LV_SYMBOL_DOWN:LV_SYMBOL_UP):
                k==9?(!strcmp(m.items[k].state,"docked")?LV_SYMBOL_HOME:LV_SYMBOL_REFRESH):LV_SYMBOL_CHARGE;
            if(laundry)symbol=visual==ROOM_VIS_DONE?LV_SYMBOL_OK:visual==ROOM_VIS_ERROR?LV_SYMBOL_WARNING:
                visual==ROOM_VIS_PAUSED?LV_SYMBOL_PAUSE:LV_SYMBOL_LOOP;
            lv_label_set_text(icon_badge[k],symbol);tint(icon_badge[k],color);
        }
        if(tile[k]){
            uint32_t background=active?(laundry?(k<5?0x142d45:0x392c1c):(k<2?0x332d1a:0x16342f)):
                visual==ROOM_VIS_DONE?0x16342f:visual==ROOM_VIS_ERROR?0x3e2028:CARD;
            lv_obj_set_style_bg_color(tile[k],lv_color_hex(background),0);
            lv_obj_set_style_border_color(tile[k],lv_color_hex(pending?AMBER:(active||visual==ROOM_VIS_DONE||visual==ROOM_VIS_ERROR)?color:EDGE),0);
        }
        if(bulb_head[k]){
            uint32_t fill=available&&active?0xffd54a:0x303b49;
            lv_obj_set_style_bg_color(bulb_head[k],lv_color_hex(fill),0);
            lv_obj_set_style_border_color(bulb_head[k],lv_color_hex(available&&active?0xffe68c:0x647386),0);
            lv_obj_set_style_bg_color(bulb_stem[k],lv_color_hex(fill),0);
            lv_obj_set_style_bg_color(bulb_base[k],lv_color_hex(available&&active?0xffd54a:0x647386),0);
        }
        if(activity_ring[k]){
            lv_obj_set_style_arc_color(activity_ring[k],lv_color_hex(color),LV_PART_INDICATOR);
            lv_arc_set_rotation(activity_ring[k],active?(unsigned)(now/3000)%360:0);
            lv_arc_set_angles(activity_ring[k],0,active?100:360);
            lv_obj_set_style_arc_opa(activity_ring[k],available?LV_OPA_COVER:LV_OPA_40,LV_PART_INDICATOR);
        }
        for(int a=0;a<6;++a)if(control[k][a]){
            enabled(control[k][a],available&&!(m.pending&(1U<<k))&&(m.items[k].actions&(1U<<a)));
            bool selected=available&&((a==0&&!strcmp(m.items[k].state,"on"))||(a==1&&!strcmp(m.items[k].state,"off")));
            lv_obj_set_style_bg_color(control[k][a],lv_color_hex(selected?(k<2?0xffd54a:MINT):EDGE),0);
            tint(control[k][a],selected?INK:TEXT);
        }
    }
    for(int k=3;k<=6;++k)if(notify_button[k-3]){
        bool on=room_audio_notify_enabled(k);
        lv_label_set_text(lv_obj_get_child(notify_button[k-3],0),on?"Notify: On":"Notify: Off");
        lv_obj_set_style_bg_color(notify_button[k-3],lv_color_hex(on?MINT:EDGE),0);
        tint(notify_button[k-3],on?INK:MUTED);
    }
    if(!ha||!m.items[10].available){
        lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(camera_note,ha?"Camera entity is unavailable in Home Assistant":"Waiting for Home Assistant");
        lv_label_set_text(camera_status,LV_SYMBOL_VIDEO "  OFFLINE");
    }
}
static void control_button(lv_obj_t *p,const char *text,int key,int action,int x,int y,int w)
{control[key][action]=button(p,text,x,y,w,42,send_event,key*6+action);enabled(control[key][action],false);}
static void build_ui(void)
{
    lv_display_add_event_cb(lv_display_get_default(),frame_event,LV_EVENT_REFR_START,NULL);
    lv_display_add_event_cb(lv_display_get_default(),frame_event,LV_EVENT_REFR_READY,NULL);
    lv_obj_t *screen=lv_screen_active();lv_obj_set_style_bg_color(screen,lv_color_hex(INK),0);
    tint(screen,TEXT);lv_obj_set_style_text_font(screen,&lv_font_montserrat_16,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(screen,gesture,LV_EVENT_GESTURE,NULL);
    label(screen,"Cottage",16,12,138,&lv_font_montserrat_24);
    health_button=button(screen,"",157,8,244,40,diagnostics_event,0);
    health_dot=panel(health_button,12,14,12,12,0xff6464,6);lv_obj_set_style_border_width(health_dot,0,0);
    connection=label(health_button,"Offline",34,11,204,&lv_font_montserrat_16);
    lv_obj_set_height(connection,24);lv_obj_remove_flag(connection,LV_OBJ_FLAG_CLICKABLE);
    settings_button=button(screen,LV_SYMBOL_SETTINGS,416,8,48,40,settings_event,0);
    const char *nav_text[]={"Climate","Home","Laundry","Camera","Ships"};
    for(int n=0;n<5;++n){
        body[n]=lv_obj_create(screen);lv_obj_remove_style_all(body[n]);lv_obj_set_pos(body[n],12,57);lv_obj_set_size(body[n],456,364);
        lv_obj_remove_flag(body[n],LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_CLICKABLE);lv_obj_add_flag(body[n],LV_OBJ_FLAG_GESTURE_BUBBLE);
        nav[n]=button(screen,nav_text[n],12+n*93,429,84,44,nav_event,n);
        lv_obj_set_style_text_font(nav[n],&lv_font_montserrat_14,0);
    }
    /* Transient error toast overlays content; no persistent footer takes space. */
    notice=label(screen,"",24,379,432,&lv_font_montserrat_14);tint(notice,AMBER);
    lv_obj_set_style_bg_color(notice,lv_color_hex(CARD),0);lv_obj_set_style_bg_opa(notice,LV_OPA_COVER,0);
    lv_obj_set_style_pad_all(notice,8,0);lv_obj_set_style_radius(notice,10,0);
    lv_label_set_long_mode(notice,LV_LABEL_LONG_DOT);lv_obj_set_height(notice,42);lv_obj_add_flag(notice,LV_OBJ_FLAG_HIDDEN);
    /* Drag the visible knob to preview 45..80 F in 1 F increments. */
    temperature_ring=lv_arc_create(body[0]);lv_obj_set_size(temperature_ring,220,220);lv_obj_set_pos(temperature_ring,118,0);
    lv_arc_set_range(temperature_ring,45,80);lv_arc_set_rotation(temperature_ring,135);
    lv_arc_set_bg_angles(temperature_ring,0,270);
    lv_arc_set_mode(temperature_ring,LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(temperature_ring,13,LV_PART_MAIN);
    lv_obj_set_style_arc_width(temperature_ring,13,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(temperature_ring,lv_color_hex(EDGE),LV_PART_MAIN);
    lv_obj_set_style_bg_color(temperature_ring,lv_color_hex(AMBER),LV_PART_KNOB);
    lv_obj_set_style_pad_all(temperature_ring,7,LV_PART_KNOB);
    lv_obj_set_style_radius(temperature_ring,LV_RADIUS_CIRCLE,LV_PART_KNOB);
    lv_obj_add_flag(temperature_ring,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(temperature_ring,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(temperature_ring,target_ring_event,LV_EVENT_VALUE_CHANGED,NULL);
    lv_obj_t *room_caption=label(body[0],"ROOM TEMPERATURE",0,48,456,&lv_font_montserrat_14);centered(room_caption);tint(room_caption,MUTED);
    temp_label=label(body[0],"-- F",0,78,456,&lv_font_montserrat_48);centered(temp_label);
    target_label=label(body[0],"Target unavailable",0,144,456,&lv_font_montserrat_20);centered(target_label);tint(target_label,AMBER);
    mode_label=label(body[0],"Controller Offline",0,219,456,&lv_font_montserrat_16);centered(mode_label);tint(mode_label,AMBER);
    reason_label=label(body[0],"Waiting for status",12,247,432,&lv_font_montserrat_14);centered(reason_label);tint(reason_label,MUTED);lv_label_set_long_mode(reason_label,LV_LABEL_LONG_DOT);lv_obj_set_height(reason_label,24);
    minus_button=button(body[0],"-1",0,271,100,42,send_event,-2);
    lv_obj_t *hint=label(body[0],"DRAG THE RING TO SET",106,281,244,&lv_font_montserrat_14);
    centered(hint);tint(hint,MUTED);
    plus_button=button(body[0],"+1",356,271,100,42,send_event,-3);
    apply_button=button(body[0],"Set Temperature",0,319,278,44,send_event,-1);
    away_button=button(body[0],"Away (50 F)",290,319,166,44,send_event,-10);
    lv_obj_set_style_bg_color(apply_button,lv_color_hex(AMBER),0);tint(apply_button,INK);
    const int rows[]={0,1,2,7,9};
    const char *names[]={"Deck lights","Breezeway lights","Blinds","Kids heat","Vacuum"};
    const char *symbols[]={LV_SYMBOL_POWER,LV_SYMBOL_POWER,LV_SYMBOL_UP,LV_SYMBOL_CHARGE,LV_SYMBOL_HOME};
    for(int row=0;row<5;++row){
        int key=rows[row];tile[key]=panel(body[1],0,row*73,456,67,CARD,13);
        lv_obj_t *p=tile[key];if(key<2){
            /* Bulb silhouette drawn locally: dark glass when off, yellow when on. */
            lv_obj_t *bulb=panel(p,8,9,36,48,CARD,0);lv_obj_set_style_bg_opa(bulb,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(bulb,0,0);
            bulb_head[key]=panel(bulb,5,0,26,29,0x303b49,13);
            bulb_stem[key]=panel(bulb,12,23,12,12,0x303b49,3);lv_obj_set_style_border_width(bulb_stem[key],0,0);
            bulb_base[key]=panel(bulb,11,37,14,4,0x647386,2);lv_obj_set_style_border_width(bulb_base[key],0,0);
        } else if(key==2)blinds_icon(p);
        else if(key==7)heater_icon(p);
        else {icon_badge[key]=label(p,symbols[row],13,21,28,&lv_font_montserrat_24);tint(icon_badge[key],BLUE);}
        label(p,names[row],52,8,186,&lv_font_montserrat_16);
        states[key]=label(p,"Unavailable",52,33,key==7?66:184,&lv_font_montserrat_14);lv_label_set_long_mode(states[key],LV_LABEL_LONG_DOT);lv_obj_set_height(states[key],22);
        if(key==2){control_button(p,"Open",key,2,240,9,66);control_button(p,"Stop",key,3,312,9,66);control_button(p,"Close",key,4,384,9,66);}
        else if(key==9){control_button(p,LV_SYMBOL_PLAY " Start",key,5,246,9,96);control_button(p,LV_SYMBOL_STOP " Stop",key,3,350,9,96);}
        else {control_button(p,"On",key,0,266,9,86);control_button(p,"Off",key,1,360,9,86);}
        if(key==7){states[8]=label(p,"--",119,28,137,&lv_font_montserrat_28);tint(states[8],TEXT);
            lv_label_set_long_mode(states[8],LV_LABEL_LONG_DOT);lv_obj_set_height(states[8],34);
            lv_obj_set_style_text_align(states[8],LV_TEXT_ALIGN_RIGHT,0);}
    }
    const char *laundry[]={"Washer 1","Washer 2","Dryer 1","Dryer 2"};
    for(int n=0;n<4;++n){
        int key=n+3;tile[key]=panel(body[2],(n%2)*234,(n/2)*184,222,174,CARD,18);
        lv_obj_t *p=tile[key];icon_badge[key]=label(p,LV_SYMBOL_LOOP,16,14,40,&lv_font_montserrat_28);tint(icon_badge[key],BLUE);
        lv_obj_t *kind=label(p,n<2?"WASH":"DRY",62,24,78,&lv_font_montserrat_14);tint(kind,MUTED);
        label(p,laundry[n],16,76,190,&lv_font_montserrat_20);
        activity_ring[key]=lv_arc_create(p);lv_obj_set_pos(activity_ring[key],148,13);lv_obj_set_size(activity_ring[key],56,56);
        lv_arc_set_bg_angles(activity_ring[key],0,360);lv_arc_set_angles(activity_ring[key],0,360);
        lv_obj_remove_style(activity_ring[key],NULL,LV_PART_KNOB);lv_obj_remove_flag(activity_ring[key],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(activity_ring[key],LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_style_arc_width(activity_ring[key],4,LV_PART_MAIN);lv_obj_set_style_arc_width(activity_ring[key],4,LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(activity_ring[key],lv_color_hex(EDGE),LV_PART_MAIN);
        states[key]=label(p,"Unavailable",16,104,190,&lv_font_montserrat_16);lv_label_set_long_mode(states[key],LV_LABEL_LONG_DOT);lv_obj_set_height(states[key],23);
        notify_button[n]=button(p,"Notify: Off",16,132,190,34,notify_event,key);lv_obj_set_style_text_font(notify_button[n],&lv_font_montserrat_14,0);
    }
    lv_obj_t *camera_title=label(body[3],"Parking lot",0,1,240,&lv_font_montserrat_24);
    (void)camera_title;
    camera_status=label(body[3],LV_SYMBOL_VIDEO "  CONNECTING",252,7,204,&lv_font_montserrat_14);tint(camera_status,MINT);
    /* Rounded child clipping creates ARGB layers in LVGL's small private heap.
     * A rectangular viewport uses normal clipping and direct image drawing. */
    camera_frame=panel(body[3],0,32,456,308,0x05090e,0);
    lv_obj_remove_flag(camera_frame,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_set_style_clip_corner(camera_frame,false,0);
    camera_image=lv_image_create(camera_frame);lv_obj_add_flag(camera_image,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(camera_image,LV_OBJ_FLAG_GESTURE_BUBBLE);
    camera_note=label(body[3],"Loading camera...",0,346,456,&lv_font_montserrat_14);centered(camera_note);tint(camera_note,MUTED);
    ship_title=label(body[4],"Ships nearby",0,0,456,&lv_font_montserrat_24);
    lv_obj_t *map_frame=panel(body[4],0,35,278,246,0x102837,0);
    lv_obj_set_style_clip_corner(map_frame,false,0);lv_obj_remove_flag(map_frame,LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    ship_map_note=label(map_frame,"Map loading...",16,96,246,&lv_font_montserrat_16);centered(ship_map_note);tint(ship_map_note,MUTED);
    ship_map=lv_image_create(map_frame);lv_obj_add_flag(ship_map,LV_OBJ_FLAG_HIDDEN);
    cottage_marker=label(map_frame,LV_SYMBOL_HOME,128,108,24,&lv_font_montserrat_16);tint(cottage_marker,MINT);lv_obj_add_flag(cottage_marker,LV_OBJ_FLAG_HIDDEN);
    for(unsigned i=0;i<ROOM_SHIP_LIMIT;++i){
        ship_markers[i]=button(map_frame,"",0,0,26,26,ship_select,i);lv_obj_set_style_radius(ship_markers[i],13,0);
        lv_obj_set_style_text_font(ship_markers[i],&lv_font_montserrat_14,0);lv_label_set_text_fmt(lv_obj_get_child(ship_markers[i],0),"%u",i+1);
        lv_obj_add_flag(ship_markers[i],LV_OBJ_FLAG_HIDDEN);
    }
    ship_detail=label(body[4],"Waiting for ship service",290,38,166,&lv_font_montserrat_16);
    lv_obj_set_height(ship_detail,240);lv_label_set_long_mode(ship_detail,LV_LABEL_LONG_DOT);
    ship_note=label(body[4],"",0,289,456,&lv_font_montserrat_14);
    button(body[4],"Previous",0,318,132,42,ship_change,-1);
    button(body[4],"Next ship",144,318,132,42,ship_change,1);
    label(body[4],"Map: OpenStreetMap",290,324,166,&lv_font_montserrat_14);
    lv_timer_create(refresh,250,NULL);select_page(0);refresh(NULL);
}
void room_display_start(void)
{
    mutex=xSemaphoreCreateMutex();commands=xQueueCreate(8,sizeof(command_t));
    ESP_ERROR_CHECK(mutex&&commands?ESP_OK:ESP_ERR_NO_MEM);
    bsp_display_cfg_t display_cfg={.lvgl_port_cfg=ESP_LVGL_PORT_INIT_CONFIG()};
    display_cfg.lvgl_port_cfg.task_stack=16384;
#if CONFIG_FREERTOS_UNICORE
    display_cfg.lvgl_port_cfg.task_affinity=0;
#else
    display_cfg.lvgl_port_cfg.task_affinity=1;
#endif
    ESP_ERROR_CHECK(bsp_display_start_with_config(&display_cfg)?ESP_OK:ESP_FAIL);
    if(bsp_display_lock(0)){
        #if CONFIG_ROOM_DISPLAY_ROTATE_180
        bsp_display_rotate(lv_display_get_default(),LV_DISPLAY_ROTATION_180);
#endif
        build_ui();bsp_display_unlock();
    }
    ESP_ERROR_CHECK(bsp_display_brightness_set(80));
    ESP_ERROR_CHECK(xTaskCreate(command_task,"room_commands",10240,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(network_task,"room_network",10240,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(camera_task,"room_camera",8192,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    ESP_LOGI(TAG,"Native 4B ready: Climate / Home / Laundry / Camera / Ships");
}
i2c_master_bus_handle_t room_display_probe_bus(int sda,int scl)
{
    if(sda!=BSP_I2C_SDA||scl!=BSP_I2C_SCL){ESP_LOGE(TAG,"4B external probe must share board I2C SDA47/SCL48; probe disabled");return NULL;}
    return bsp_i2c_get_handle();
}
#else
void room_display_probe_sample(float temperature_f) {(void)temperature_f;}
void room_display_start(void) {}
cJSON *room_display_status(void){return cJSON_CreateObject();}
bool room_display_camera_fill(void){return false;}
bool room_display_request_away(bool enabled){(void)enabled;return false;}
bool room_display_request_target(float target){(void)target;return false;}
void room_display_set_camera_fill(bool fill){(void)fill;}
i2c_master_bus_handle_t room_display_probe_bus(int sda,int scl) {return NULL;}
#endif


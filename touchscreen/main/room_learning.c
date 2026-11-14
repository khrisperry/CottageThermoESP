/* Passive, review-only learning. Never sends control commands or rewrites calibration.
 * The room owns weather/power/probe data; controller is queried read-only on LAN. */
#include "room_learning.h"
#include "room_learning_policy.h"
#include "room_controller.h"
#include "room_ha.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RL_RING_PSRAM 2016U       /* 7 days at one sample every 5 minutes */
#define RL_RING_FALLBACK 288U     /* 24 hours */
#define RL_PERSIST_MS 21600000ULL /* 6 hours; modest flash wear */
#define RL_MODEL_MAGIC 0x524C4D31U
#define RL_MODEL_SCHEMA 1U
#define RL_PROBE_MAX_AGE_MS 45000ULL

enum {
    RL_ROOM=1U, RL_CONTROLLER=2U, RL_LIVE=4U, RL_ENCODER=8U,
    RL_POWER=16U, RL_WEATHER=32U, RL_STABLE=64U, RL_CANDIDATE=128U
};
typedef struct {
    uint32_t count;
    float sum_f, min_f, max_f;
} rl_bin_t;
typedef struct {
    uint32_t magic, schema, total_candidates;
    rl_bin_t bins[RL_BINS];
} rl_model_t;
typedef struct {
    int64_t unix_s;
    uint32_t uptime_s;
    float room_f, raw_f, target_f, commanded_level, encoder_deg;
    float outside_f, cloud_pct, power_w, current_a;
    uint8_t flags;
    char stage[24];
    char mode[16];
} rl_entry_t;

static const char *TAG="room_learning";
static SemaphoreHandle_t lock;
static rl_entry_t *ring;
static unsigned capacity, next, count, total_rows;
static rl_model_t model;
static float last_probe_f=NAN,last_probe_raw=NAN;
static uint64_t last_probe_ms;
static uint64_t unchanged_from_ms, last_persist_ms;
static float unchanged_level=NAN, unchanged_target=NAN;
static unsigned persist_count;
static bool summary_dirty, running;

static cJSON *field(const cJSON *j,const char *name)
{ return cJSON_GetObjectItemCaseSensitive(j,name); }
static bool number(const cJSON *j,const char *name,float *out)
{
    cJSON *v=field(j,name);
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;
    *out=(float)v->valuedouble;return true;
}
static bool yes(const cJSON *j,const char *name){return cJSON_IsTrue(field(j,name));}
static const char *word(const cJSON *j,const char *name)
{
    cJSON *v=field(j,name);return cJSON_IsString(v)?v->valuestring:"";
}
static void safe_word(char *dest,size_t n,const char *s)
{
    /* No untrusted/CSV formula data; all fields are known categorical values. */
    snprintf(dest,n,"%s",s);
}
static void load_model(void)
{
    memset(&model,0,sizeof(model));model.magic=RL_MODEL_MAGIC;model.schema=RL_MODEL_SCHEMA;
    nvs_handle_t nvs;
    if(nvs_open("room_learning",NVS_READONLY,&nvs)!=ESP_OK)return;
    rl_model_t previous;size_t bytes=sizeof(previous);
    if(nvs_get_blob(nvs,"summary",&previous,&bytes)==ESP_OK &&
       bytes==sizeof(previous) && previous.magic==RL_MODEL_MAGIC &&
       previous.schema==RL_MODEL_SCHEMA) {
        bool valid=true;
        for(unsigned i=0;i<RL_BINS;++i){
            const rl_bin_t *b=&previous.bins[i];
            if((b->count && (!isfinite(b->sum_f)||!isfinite(b->min_f)||!isfinite(b->max_f)||
                              b->min_f>b->max_f||b->min_f<32||b->max_f>110)) ||
               b->count>10000000U)valid=false;
        }
        if(valid)model=previous;
    }
    nvs_close(nvs);
}
static bool persist_summary(const rl_model_t *summary)
{
    nvs_handle_t nvs;
    if(nvs_open("room_learning",NVS_READWRITE,&nvs)!=ESP_OK)return false;
    esp_err_t err=nvs_set_blob(nvs,"summary",summary,sizeof(*summary));
    if(err==ESP_OK)err=nvs_commit(nvs);
    nvs_close(nvs);
    return err==ESP_OK;
}

void room_learning_probe(float corrected_f,float raw_f)
{
    if(!lock||!isfinite(corrected_f)||!isfinite(raw_f))return;
    xSemaphoreTake(lock,portMAX_DELAY);
    last_probe_f=corrected_f;last_probe_raw=raw_f;
    last_probe_ms=(uint64_t)(esp_timer_get_time()/1000);
    xSemaphoreGive(lock);
}
static void append_locked(const rl_entry_t *e)
{
    if(!ring||!capacity)return;
    ring[next]=*e;next=(next+1)%capacity;
    if(count<capacity)++count;
}
static void observe(void)
{
    rl_entry_t e={.room_f=NAN,.raw_f=NAN,.target_f=NAN,
        .commanded_level=NAN,.encoder_deg=NAN,.outside_f=NAN,
        .cloud_pct=NAN,.power_w=NAN,.current_a=NAN};
    uint64_t now=(uint64_t)(esp_timer_get_time()/1000);
    e.uptime_s=(uint32_t)(now/1000);
    time_t wall=time(NULL);
    e.unix_s=wall>1700000000?(int64_t)wall:0;
    safe_word(e.mode,sizeof(e.mode),"unknown");
    safe_word(e.stage,sizeof(e.stage),"unknown");

    xSemaphoreTake(lock,portMAX_DELAY);
    if(last_probe_ms && now>=last_probe_ms &&
       now-last_probe_ms<=RL_PROBE_MAX_AGE_MS){
        e.room_f=last_probe_f;e.raw_f=last_probe_raw;e.flags|=RL_ROOM;
    }
    xSemaphoreGive(lock);

    cJSON *control=room_controller_learning_status();
    if(control){
        e.flags|=RL_CONTROLLER;
        (void)number(control,"commanded_level",&e.commanded_level);
        (void)number(control,"target_f",&e.target_f);
        const char *mode=word(control,"mode");
        if(!strcmp(mode,"thermostat")||!strcmp(mode,"hold")||
           !strcmp(mode,"manual")||!strcmp(mode,"direct"))
            safe_word(e.mode,sizeof(e.mode),mode);
        bool healthy=yes(control,"live_active") && !yes(control,"live_fault") &&
                     !yes(control,"live_verifying") && !yes(control,"simulation") &&
                     yes(control,"position_synchronized") &&
                     !strcmp(e.mode,"thermostat");
        if(healthy)e.flags|=RL_LIVE;
        cJSON *encoder=field(control,"encoder");
        if(yes(encoder,"angle_valid")&&number(encoder,"angle_deg",&e.encoder_deg))
            e.flags|=RL_ENCODER;
        cJSON_Delete(control);
    }
    cJSON *furnace=room_ha_furnace_status();
    if(furnace){
        const char *stage=word(furnace,"activity");
        const char *known[]={"standby","startup","likely_heating","likely_cooldown",
                             "transition","unclassified_load","unknown"};
        for(unsigned i=0;i<sizeof(known)/sizeof(known[0]);++i)
            if(!strcmp(stage,known[i]))safe_word(e.stage,sizeof(e.stage),known[i]);
        cJSON *power=field(furnace,"power"),*current=field(furnace,"current");
        bool good=yes(furnace,"enabled")&&yes(furnace,"installed")&&
                  yes(power,"available")&&yes(power,"source_fresh")&&
                  number(power,"value",&e.power_w);
        if(good)e.flags|=RL_POWER;
        if(yes(current,"available")&&yes(current,"source_fresh"))
            (void)number(current,"value",&e.current_a);
        cJSON_Delete(furnace);
    }
    cJSON *weather=room_ha_weather_status();
    if(weather){
        if(yes(weather,"available")&&yes(weather,"source_fresh")&&
           number(weather,"temperature_f",&e.outside_f))e.flags|=RL_WEATHER;
        if(e.flags&RL_WEATHER)(void)number(weather,"cloud_coverage_percent",&e.cloud_pct);
        cJSON_Delete(weather);
    }

    bool pending_persist=false;rl_model_t to_save;
    xSemaphoreTake(lock,portMAX_DELAY);
    bool normal=(e.flags&(RL_ROOM|RL_CONTROLLER|RL_LIVE|RL_ENCODER))==
                (RL_ROOM|RL_CONTROLLER|RL_LIVE|RL_ENCODER) &&
                isfinite(e.commanded_level)&&isfinite(e.target_f);
    if(!normal){
        unchanged_from_ms=0;unchanged_level=NAN;unchanged_target=NAN;
    }else if(!isfinite(unchanged_level) || !isfinite(unchanged_target) ||
            fabsf(unchanged_level-e.commanded_level)>0.04f ||
            fabsf(unchanged_target-e.target_f)>0.04f){
        unchanged_from_ms=now;unchanged_level=e.commanded_level;
        unchanged_target=e.target_f;
    }
    unsigned consecutive=0;float low=e.room_f,high=e.room_f;
    if(normal && ring && capacity>=6){
        for(unsigned i=0;i<6 && i<count;++i){
            const rl_entry_t *old=&ring[(next+capacity-1-i)%capacity];
            if(!(old->flags&RL_ROOM)||!(old->flags&RL_LIVE)||
               !isfinite(old->commanded_level)||
               fabsf(old->commanded_level-e.commanded_level)>0.04f ||
               !isfinite(old->target_f)||
               fabsf(old->target_f-e.target_f)>0.04f)break;
            if(old->room_f<low)low=old->room_f;
            if(old->room_f>high)high=old->room_f;
            ++consecutive;
        }
    }
    if(normal&&rl_settled(now,unchanged_from_ms,consecutive,high-low))
        e.flags|=RL_STABLE;
    /* These are *review candidates* only, not equilibrium proofs or motion. */
    int bin=rl_half_step_bin(e.commanded_level);
    if(bin>=0&&(e.flags&(RL_STABLE|RL_POWER|RL_WEATHER))==
                 (RL_STABLE|RL_POWER|RL_WEATHER)){
        e.flags|=RL_CANDIDATE;
        rl_bin_t *b=&model.bins[bin];
        if(b->count==0)b->min_f=b->max_f=e.room_f;
        if(b->count<10000000U) {++b->count;b->sum_f+=e.room_f;
            if(e.room_f<b->min_f)b->min_f=e.room_f;
            if(e.room_f>b->max_f)b->max_f=e.room_f;
            if(model.total_candidates<UINT32_MAX)++model.total_candidates;
            summary_dirty=true;}
    }
    append_locked(&e);
    ++total_rows;
    if(summary_dirty && now>=last_persist_ms &&
       now-last_persist_ms>=RL_PERSIST_MS){
        to_save=model;pending_persist=true;
    }
    xSemaphoreGive(lock);
    if(pending_persist){
        if(persist_summary(&to_save)){
            ++persist_count;last_persist_ms=now;
            xSemaphoreTake(lock,portMAX_DELAY);
            /* Only this observer task mutates the model. */
            summary_dirty=false;
            xSemaphoreGive(lock);
        }else ESP_LOGW(TAG,"Summary checkpoint failed; observations continue in RAM");
    }
    ESP_LOGI(TAG,"read-only observation room=%.1f dial_command=%.2f stage=%s flags=0x%02x",
             e.room_f,e.commanded_level,e.stage,e.flags);
}
static void task(void *unused)
{
    (void)unused;
    vTaskDelay(pdMS_TO_TICKS(45000));
    for(;;){observe();vTaskDelay(pdMS_TO_TICKS(300000));}
}
void room_learning_start(void)
{
    lock=xSemaphoreCreateMutex();
    if(!lock){ESP_LOGW(TAG,"Cannot allocate observation lock");return;}
    capacity=RL_RING_PSRAM;
    ring=heap_caps_malloc(capacity*sizeof(*ring),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!ring){
        capacity=RL_RING_FALLBACK;
        ring=calloc(capacity,sizeof(*ring));
    }
    if(!ring){capacity=0;ESP_LOGW(TAG,"No RAM history; only summary collection");}
    load_model();
    last_persist_ms=(uint64_t)(esp_timer_get_time()/1000);
    running=xTaskCreate(task,"passive_learning",9216,NULL,1,NULL)==pdPASS;
    if(!running)ESP_LOGW(TAG,"Observation task could not start");
}
cJSON *room_learning_status(void)
{
    cJSON *j=cJSON_CreateObject();if(!j)return NULL;
    cJSON_AddStringToObject(j,"mode","passive_review_only");
    cJSON_AddBoolToObject(j,"collecting",running);
    cJSON_AddBoolToObject(j,"automatic_adjustments_enabled",false);
    cJSON_AddStringToObject(j,"dial_source","controller_commanded_level_not_measured_knob");
    cJSON_AddNumberToObject(j,"interval_seconds",300);
    cJSON_AddStringToObject(j,"csv","/api/learning.csv");
    if(!lock)return j;
    xSemaphoreTake(lock,portMAX_DELAY);
    cJSON_AddNumberToObject(j,"samples_in_ram",count);
    cJSON_AddNumberToObject(j,"ram_capacity",capacity);
    cJSON_AddNumberToObject(j,"samples_since_boot",total_rows);
    cJSON_AddNumberToObject(j,"candidate_samples_total",model.total_candidates);
    cJSON_AddNumberToObject(j,"checkpoints_this_boot",persist_count);
    cJSON *bins=cJSON_AddArrayToObject(j,"provisional_bins");
    for(unsigned i=0;i<RL_BINS;++i){
        cJSON *b=cJSON_CreateObject();
        cJSON_AddNumberToObject(b,"dial_level",1.0f+0.5f*i);
        cJSON_AddNumberToObject(b,"samples",model.bins[i].count);
        if(model.bins[i].count){
            cJSON_AddNumberToObject(b,"mean_temperature_f",model.bins[i].sum_f/model.bins[i].count);
            cJSON_AddNumberToObject(b,"min_temperature_f",model.bins[i].min_f);
            cJSON_AddNumberToObject(b,"max_temperature_f",model.bins[i].max_f);
        }
        cJSON_AddItemToArray(bins,b);
    }
    if(ring&&count){
        rl_entry_t *latest=&ring[(next+capacity-1)%capacity];
        cJSON *s=cJSON_AddObjectToObject(j,"latest");
        cJSON_AddNumberToObject(s,"unix_s",(double)latest->unix_s);
        cJSON_AddNumberToObject(s,"uptime_s",latest->uptime_s);
        cJSON_AddNumberToObject(s,"flags",latest->flags);
        cJSON_AddStringToObject(s,"stage",latest->stage);
        cJSON_AddStringToObject(s,"mode",latest->mode);
        if(isfinite(latest->room_f))cJSON_AddNumberToObject(s,"room_f",latest->room_f);
        if(isfinite(latest->commanded_level))cJSON_AddNumberToObject(s,"commanded_level",latest->commanded_level);
        cJSON_AddBoolToObject(s,"review_candidate",(latest->flags&RL_CANDIDATE)!=0);
    }
    xSemaphoreGive(lock);
    return j;
}
static const char *csv_number(float f,char *b,size_t n)
{
    if(!isfinite(f)){b[0]=0;return b;}
    snprintf(b,n,"%.3f",f);return b;
}
esp_err_t room_learning_csv(httpd_req_t *r)
{
    if(!lock)return httpd_resp_send_err(r,HTTPD_503_SERVICE_UNAVAILABLE,"Learning not started");
    httpd_resp_set_type(r,"text/csv");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    httpd_resp_set_hdr(r,"Content-Disposition","attachment; filename=cottage-thermostat-learning.csv");
    esp_err_t err=httpd_resp_sendstr_chunk(r,
      "unix_s,uptime_s,room_f,raw_f,target_f,commanded_dial,encoder_angle_deg,"
      "outdoor_f,cloud_percent,power_w,current_a,controller_mode,furnace_stage,"
      "room_fresh,controller_lan,live,encoder_valid,power_fresh,weather_fresh,"
      "dial_settled,review_candidate\n");
    xSemaphoreTake(lock,portMAX_DELAY);
    unsigned snapshot_count=count,begin=capacity?(next+capacity-count)%capacity:0;
    xSemaphoreGive(lock);
    for(unsigned i=0;i<snapshot_count&&err==ESP_OK;++i){
        rl_entry_t e;
        xSemaphoreTake(lock,portMAX_DELAY);
        e=ring[(begin+i)%capacity];
        xSemaphoreGive(lock);
        char a[10][24],line[512];
        const float nums[]={e.room_f,e.raw_f,e.target_f,e.commanded_level,
                            e.encoder_deg,e.outside_f,e.cloud_pct,e.power_w,e.current_a};
        for(unsigned k=0;k<9;++k)csv_number(nums[k],a[k],sizeof(a[k]));
        snprintf(line,sizeof(line),
          "%lld,%u,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%u,%u,%u,%u,%u,%u,%u,%u\n",
          (long long)e.unix_s,e.uptime_s,
          a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8],
          e.mode,e.stage,
          !!(e.flags&RL_ROOM),!!(e.flags&RL_CONTROLLER),!!(e.flags&RL_LIVE),
          !!(e.flags&RL_ENCODER),!!(e.flags&RL_POWER),!!(e.flags&RL_WEATHER),
          !!(e.flags&RL_STABLE),!!(e.flags&RL_CANDIDATE));
        err=httpd_resp_sendstr_chunk(r,line);
    }
    if(err==ESP_OK)err=httpd_resp_send_chunk(r,NULL,0);
    return err;
}

/* Tests the production HTTP workflow with fake hardware boundaries. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
#include "controller_setup.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define HTTPD_400_BAD_REQUEST "400"
#define TAG "test"
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0
#define ESP_LOGE(...) ((void)0)
typedef struct {bool online,magnet_detected,too_weak,too_strong;uint64_t at_ms;int raw_angle;} controller_encoder_sample_t;
typedef struct {const char *request;char response[256];} httpd_req_t;
static th_state_t state;static controller_setup_t setup,persisted;static bool go_active,pwm,armed;
static int last_pulse=-1,lock,store,moves;static bool fail_save;static uint64_t armed_until,clock_ms=100000;
static uint64_t now_ms(void){return clock_ms;}
static void event(const char *text){(void)text;}
static int xSemaphoreTake(int x,int y){(void)x;(void)y;return 1;}
static void xSemaphoreGive(int x){(void)x;}
#define portMAX_DELAY 0
static int nvs_set_blob(int h,const char *key,const void *p,size_t n){(void)h;(void)key;assert(n==sizeof(setup));if(fail_save)return 1;memcpy(&persisted,p,n);return 0;}
static int nvs_commit(int h){(void)h;return 0;}
static int ledc_stop(int a,int b,int c){(void)a;(void)b;(void)c;return 0;}
static const char *esp_err_to_name(int err){(void)err;return "error";}
static esp_err_t pulse(int us){if(!controller_setup_contains(&setup,us))return ESP_ERR_INVALID_ARG;pwm=true;last_pulse=us;moves++;return ESP_OK;}
static void release(void)
{
    if(pwm){int err=ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);if(err!=ESP_OK)(void)esp_err_to_name(err);}
    pwm=false;last_pulse=-1;armed=false;go_active=false;state.simulated=true;th_release(&state);
}
static cJSON *body(httpd_req_t *r){return cJSON_Parse(r->request);}
static int httpd_resp_send_err(httpd_req_t *r,const char *status,const char *error){(void)status;snprintf(r->response,sizeof(r->response),"%s",error);return 400;}
static int httpd_resp_sendstr(httpd_req_t *r,const char *text){snprintf(r->response,sizeof(r->response),"%s",text);return 0;}
static controller_encoder_sample_t controller_encoder_snapshot(void){return (controller_encoder_sample_t){.online=true,.magnet_detected=true,.at_ms=clock_ms,.raw_angle=2048};}
static bool number(cJSON *j,const char *key,double *out){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;*out=v->valuedouble;return true;}
static bool text_is(cJSON *j,const char *key,const char *s){cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)&&!strcmp(v->valuestring,s);}
#include "../controller/main/controller_setup_http.inc"
static int request(const char *json,const char *error)
{
    httpd_req_t r={.request=json};int result=setup_post(&r);
    if(error){assert(result==400);if(!strstr(r.response,error)){fprintf(stderr,"expected '%s', got '%s'\n",error,r.response);abort();}}
    else {if(result){fprintf(stderr,"unexpected error: %s\n",r.response);abort();}assert(!strcmp(r.response,"{\"ok\":true}"));}
    return result;
}
int main(void)
{
    setup=controller_setup_defaults();th_config_t c=th_defaults();th_init(&state,&c,true);
    request("{\"action\":\"jog\",\"delta_us\":10}","Test Mode");assert(moves==0);
    request("{\"action\":\"test\"}","Confirm");assert(!armed);
    request("{\"action\":\"test\",\"confirm\":\"unloaded_bench\"}",NULL);assert(armed&&!pwm&&state.config.mode==TH_HOLD);
    request("{\"action\":\"jog\",\"delta_us\":10}","starting pulse");
    request("{\"action\":\"capture\",\"kind\":\"start\"}","move to");
    request("{\"action\":\"pulse\",\"us\":-2147483649}","500 to 2500");
    request("{\"action\":\"pulse\",\"us\":600}",NULL);request("{\"action\":\"capture\",\"kind\":\"start\"}",NULL);
    request("{\"action\":\"capture\",\"kind\":\"end\"}","Limits must differ");
    request("{\"action\":\"pulse\",\"us\":2400}",NULL);request("{\"action\":\"capture\",\"kind\":\"end\"}",NULL);
    /* Correct an end limit captured too early without destroying the start. */
    request("{\"action\":\"edit_limit\",\"kind\":\"end\"}","Confirm");
    request("{\"action\":\"edit_limit\",\"kind\":\"unknown\",\"confirm\":\"recalibrate_limit\"}","Choose");
    assert(setup.start_us==600 && setup.end_us==2400 && armed && pwm);
    request("{\"action\":\"edit_limit\",\"kind\":\"end\",\"confirm\":\"recalibrate_limit\"}",NULL);
    assert(setup.start_us==600 && setup.end_us==0 && persisted.end_us==0);
    assert(!armed && !pwm && !go_active && last_pulse==-1 && state.config.mode==TH_HOLD);
    assert(controller_setup_contains(&setup,2500) && controller_setup_ready(&setup));
    request("{\"action\":\"pulse\",\"us\":2450}","Test Mode");
    request("{\"action\":\"test\",\"confirm\":\"unloaded_bench\"}",NULL);
    request("{\"action\":\"pulse\",\"us\":2450}",NULL);
    request("{\"action\":\"capture\",\"kind\":\"end\"}",NULL);
    assert(setup.start_us==600 && setup.end_us==2450);
    int count=moves;request("{\"action\":\"jog\",\"delta_us\":10}","outside");assert(moves==count);
    request("{\"action\":\"pulse\",\"us\":500}","outside");
    for(int i=0;i<8;i++) {
        char json[120];snprintf(json,sizeof(json),"{\"action\":\"pulse\",\"us\":%d}",700+i*200);request(json,NULL);
        snprintf(json,sizeof(json),"{\"action\":\"capture\",\"kind\":\"position\",\"index\":%d}",i);request(json,NULL);
    }
    assert(setup.angle_mask==255 && setup.angle_deg[3]==180);assert(persisted.pulse_us[7]==2100);
    fail_save=true;request("{\"action\":\"temperatures\",\"values\":[45,52,59,66,73,80]}","Could not save");assert(setup.temperature_mask==0);fail_save=false;
    request("{\"action\":\"temperatures\",\"values\":[45,52,52,66,73,80]}","increase");
    request("{\"action\":\"temperatures\",\"values\":[45,52,59,66,73,80]}",NULL);assert(!controller_setup_ready(&setup));
    /* Revising the start keeps every captured point and estimated temperature. */
    request("{\"action\":\"edit_limit\",\"kind\":\"start\",\"confirm\":\"recalibrate_limit\"}",NULL);
    assert(setup.start_us==0 && setup.end_us==2450 && persisted.start_us==0);
    assert(setup.pulse_us[7]==2100 && setup.angle_mask==255 && setup.temperature_mask==63);
    assert(setup.temperature_f[4]==73 && !armed && !pwm && controller_setup_ready(&setup));
    request("{\"action\":\"go\",\"position\":3,\"confirm\":\"start_bench\"}","Capture both travel limits");
    request("{\"action\":\"test\",\"confirm\":\"unloaded_bench\"}",NULL);
    request("{\"action\":\"pulse\",\"us\":550}",NULL);
    request("{\"action\":\"capture\",\"kind\":\"start\"}",NULL);
    assert(setup.start_us==550 && setup.end_us==2450 && !controller_setup_ready(&setup));
    /* Failed NVS writes must leave prior calibration intact and movement off. */
    fail_save=true;
    request("{\"action\":\"edit_limit\",\"kind\":\"start\",\"confirm\":\"recalibrate_limit\"}","Could not reopen");
    assert(setup.start_us==550 && persisted.start_us==550 && !armed && !pwm);
    fail_save=false;
    request("{\"action\":\"test\",\"confirm\":\"unloaded_bench\"}",NULL);
    cJSON *saved=setup_json();assert(cJSON_IsTrue(cJSON_GetObjectItem(saved,"ready")));cJSON_Delete(saved);
    request("{\"action\":\"go\",\"position\":0,\"confirm\":\"start_bench\"}","1 through 6");
    request("{\"action\":\"go\",\"position\":3,\"confirm\":\"start_bench\"}","saved starting");
    request("{\"action\":\"pulse\",\"us\":1300}",NULL);
    request("{\"action\":\"go\",\"position\":3,\"confirm\":\"start_bench\"}","fresh temperature");assert(!go_active&&state.simulated);
    assert(th_sample(&state,TH_ROOM,63,0,clock_ms));state.config.target_f=44;
    request("{\"action\":\"go\",\"position\":3,\"confirm\":\"start_bench\"}","calibration cannot");state.config.target_f=65;
    count=moves;request("{\"action\":\"go\",\"position\":3,\"confirm\":\"start_bench\"}",NULL);
    assert(go_active&&!state.simulated&&state.synchronized&&moves==count);assert(state.config.map_count==6);
    request("{\"action\":\"jog\",\"delta_us\":10}","Test Mode");
    request("{\"action\":\"temperatures\",\"values\":[45,52,59,66,73,80]}","Stop Go");
    th_action_t a=th_tick(&state,clock_ms);assert(a.move&&a.level==3.25f&&a.pulse_us==1350);
    request("{\"action\":\"test\",\"confirm\":\"unloaded_bench\"}",NULL);assert(!go_active&&!pwm&&state.simulated);
    clock_ms=armed_until;request("{\"action\":\"pulse\",\"us\":1300}","Test Mode");
    request("{\"action\":\"reset\",\"confirm\":\"reset_calibration\"}",NULL);assert(!setup.start_us&&!setup.temperature_mask&&!armed&&!pwm);
    puts("PASS: setup HTTP: selective limit edits, saved-position/temperature preservation, disarm, NVS failure, travel enforcement, Go and timeout");
}

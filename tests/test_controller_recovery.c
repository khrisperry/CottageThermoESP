#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include "controller_recovery.h"

typedef int esp_err_t;
typedef struct {const char *request;char response[256];} httpd_req_t;
#define HTTPD_400_BAD_REQUEST "400"
#define HTTPD_500_INTERNAL_SERVER_ERROR "500"
#define pdPASS 1
#define portMAX_DELAY 0
#define pdMS_TO_TICKS(x) (x)
static bool restart_pending,live_selected=true,pwm=true,go_active=true,fail_task;
static int lock,task_count,restarts,releases;
static void (*scheduled)(void *);
static cJSON *body(httpd_req_t *r){return cJSON_Parse(r->request);}
static bool text_is(cJSON *j,const char *key,const char *value)
{cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)&&!strcmp(v->valuestring,value);}
static int xSemaphoreTake(int l,int timeout){(void)l;(void)timeout;return 1;}
static void xSemaphoreGive(int l){(void)l;}
static void vTaskDelay(int delay){assert(delay==1500);}
static void esp_restart(void){assert(restart_pending&&!pwm&&!go_active);++restarts;}
static int xTaskCreate(void (*task)(void *),const char *name,int stack,void *arg,int priority,void *handle)
{
    (void)name;(void)stack;(void)arg;(void)priority;(void)handle;
    if(fail_task)return 0;
    ++task_count;scheduled=task;return pdPASS;
}
static void release(void){pwm=false;go_active=false;++releases;}
static void event(const char *reason){assert(reason&&*reason);}
static int httpd_resp_send_err(httpd_req_t *r,const char *code,const char *message)
{snprintf(r->response,sizeof(r->response),"%s",message);return !strcmp(code,"400")?400:500;}
static int httpd_resp_sendstr(httpd_req_t *r,const char *message)
{snprintf(r->response,sizeof(r->response),"%s",message);return 0;}
#include "../controller/main/controller_restart.inc"

int main(void)
{
    const uint64_t start=1000,timeout=CONTROLLER_RECOVERY_MS;
    controller_recovery_t r={0};
    /* Live at boot with NO first connection still recovers after 30 min. */
    assert(!controller_recovery_due(&r,true,start));
    assert(!controller_recovery_due(&r,true,start+timeout-1));
    assert(controller_recovery_due(&r,true,start+timeout));
    /* Either accepted transport's sensor report resets the whole window. */
    controller_recovery_sample(&r,start+timeout);
    assert(!controller_recovery_due(&r,true,start+timeout));
    assert(controller_recovery_remaining(&r,start+timeout)==timeout);
    assert(!controller_recovery_due(&r,true,start+2*timeout-1));
    assert(controller_recovery_due(&r,true,start+2*timeout));
    /* Status/browser traffic has no policy hook; cannot extend the timer.
     * Service and explicit faults disable recovery, even after long outages. */
    assert(!controller_recovery_due(&r,false,start+3*timeout));
    assert(!controller_recovery_due(&r,false,start+9*timeout));
    assert(!controller_recovery_due(&r,true,start+9*timeout));
    assert(controller_recovery_remaining(&r,start+9*timeout)==timeout);
    /* Large uptime is not truncated to 32 bits. */
    r=(controller_recovery_t){0};uint64_t later=1ULL<<40;
    assert(!controller_recovery_due(&r,true,later));
    assert(controller_recovery_due(&r,true,later+timeout));

    httpd_req_t req={.request="{}"};
    assert(restart_post(&req)==400&&task_count==0&&pwm);
    req.request="{\"confirm\":\"restart_controller\"}";
    fail_task=true;
    assert(restart_post(&req)==500&&!restart_pending&&pwm&&go_active&&releases==0);
    fail_task=false;
    assert(restart_post(&req)==0&&strstr(req.response,"\"restarting\":true"));
    assert(restart_pending&&!pwm&&!go_active&&live_selected&&restarts==0&&task_count==1);
    /* Repeated scheduling never creates parallel reboot tasks. */
    assert(schedule_controller_restart("duplicate")&&task_count==1);
    scheduled(NULL);assert(restarts==1&&live_selected);
    /* A restart in Service must preserve Service too. */
    restart_pending=false;live_selected=false;pwm=true;
    assert(restart_post(&req)==0&&!pwm&&!live_selected);scheduled(NULL);
    assert(restarts==2);
    puts("PASS: 30-minute boot/outage recovery, fresh samples, Service exemption, confirmed web restart, PWM-off, preserved mode, deferred reboot and task failure");
}

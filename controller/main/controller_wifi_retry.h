#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {bool connecting;uint32_t started,due;} controller_wifi_retry_t;
typedef enum { CONTROLLER_WIFI_WAIT,CONTROLLER_WIFI_CONNECT,CONTROLLER_WIFI_ABORT } controller_wifi_action_t;
static inline controller_wifi_action_t controller_wifi_retry_tick(controller_wifi_retry_t *s,uint32_t now,bool radio)
{
    uint32_t delay=radio?20000U:2000U;
    if(s->connecting){
        if((uint32_t)(now-s->started)<(radio?5000U:15000U))return CONTROLLER_WIFI_WAIT;
        s->connecting=false;s->due=now+delay;
        return CONTROLLER_WIFI_ABORT;
    }
    if((int32_t)(now-s->due)<0)return CONTROLLER_WIFI_WAIT;
    s->connecting=true;s->started=now;s->due=now+delay;
    return CONTROLLER_WIFI_CONNECT;
}

#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef enum { ROOM_WIFI_WAIT,ROOM_WIFI_CONNECT,ROOM_WIFI_ABORT } room_wifi_retry_t;
static inline int64_t room_wifi_retry_delay(bool radio)
{return radio?20000000LL:2000000LL;}
static inline room_wifi_retry_t room_wifi_retry_action(bool connecting,int64_t now,
    int64_t started,int64_t due,bool radio)
{
    /* An active attempt's deadline is independent of the next retry time. */
    if(connecting)return now-started>=(radio?5000000LL:15000000LL)?ROOM_WIFI_ABORT:ROOM_WIFI_WAIT;
    return now>=due?ROOM_WIFI_CONNECT:ROOM_WIFI_WAIT;
}
static inline void room_wifi_retry_abort(bool *connecting,int64_t *due,int64_t now,bool radio)
{
    /* The driver may already be disconnected and emit no further event. */
    *connecting=false;*due=now+room_wifi_retry_delay(radio);
}

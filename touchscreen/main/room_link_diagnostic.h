#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Independent, observation-only trace: never changes transport, safety or PWM. */
#define RLD_CAPACITY 48U
#define RLD_REPEAT_SUPPRESS_MS 30000ULL

typedef enum {
    RLD_LINK_OK, RLD_LINK_FAIL, RLD_RADIO_OK, RLD_RADIO_FAIL,
    RLD_SWITCH_RADIO, RLD_SWITCH_LAN, RLD_LEARN_OK, RLD_LEARN_FAIL,
    RLD_DISCOVERY_FAIL, RLD_LAN_GRACE_START, RLD_LAN_GRACE_RECOVERED,
    RLD_LAN_GRACE_EXPIRED, RLD_COUNT
} rld_kind_t;
typedef struct {
    uint64_t uptime_ms;
    int32_t error, http_status, duration_ms, rssi, channel;
    bool room_wifi, station_associated, espnow_selected;
    char operation[20], reason[32];
} rld_event_t;
typedef struct {
    uint32_t counters[RLD_COUNT];
    rld_event_t events[RLD_CAPACITY];
    uint32_t next, count;
} rld_log_t;
static inline const char *rld_kind_name(rld_kind_t k)
{
    static const char *const names[RLD_COUNT]={
        "link_http_ok","link_http_fail","espnow_probe_ok","espnow_probe_fail",
        "switched_to_espnow","returned_to_lan","learning_lan_ok",
        "learning_lan_fail","discovery_fail",
        "lan_grace_started","lan_grace_recovered","lan_grace_expired"};
    return k>=0 && k<RLD_COUNT?names[k]:"unknown";
}
/* Still count every attempt. Repeated identical link failures are coalesced
 * in the bounded diagnostic ring so the event that initiated fallback remains
 * visible for review. The ring is RAM only and never persisted to flash. */
static inline void rld_record(rld_log_t *log,rld_kind_t kind,
                              const rld_event_t *entry,bool keep_event)
{
    if(!log||!entry||kind<0||kind>=RLD_COUNT)return;
    if(log->counters[kind]<UINT32_MAX)++log->counters[kind];
    if(!keep_event)return;
    if(kind==RLD_LINK_FAIL || kind==RLD_RADIO_FAIL){
        if(log->count){
            const rld_event_t *last=&log->events[(log->next+RLD_CAPACITY-1)%RLD_CAPACITY];
            if(!strcmp(last->operation,entry->operation) &&
               !strcmp(last->reason,entry->reason) &&
               entry->uptime_ms>=last->uptime_ms &&
               entry->uptime_ms-last->uptime_ms<RLD_REPEAT_SUPPRESS_MS)
                return;
        }
    }
    log->events[log->next]=*entry;
    log->next=(log->next+1)%RLD_CAPACITY;
    if(log->count<RLD_CAPACITY)++log->count;
}

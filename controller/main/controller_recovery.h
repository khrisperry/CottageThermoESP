#pragma once
#include <stdbool.h>
#include <stdint.h>

#define CONTROLLER_RECOVERY_MS (30ULL * 60 * 1000)
typedef struct {
    bool watching, sample_received;
    uint64_t live_since_ms, last_sample_ms;
} controller_recovery_t;

/* Call only after accepting a primary thermostat sample, over either link.
 * Browser/status traffic and rejected packets do not prove sensor delivery. */
static inline void controller_recovery_sample(controller_recovery_t *r,uint64_t now)
{
    r->sample_received=true;
    r->last_sample_ms=now;
}
static inline uint64_t controller_recovery_remaining(const controller_recovery_t *r,uint64_t now)
{
    uint64_t since=r->live_since_ms;
    if(!r->watching)return CONTROLLER_RECOVERY_MS;
    if(r->sample_received && r->last_sample_ms>since)since=r->last_sample_ms;
    uint64_t elapsed=now>=since?now-since:0;
    return elapsed>=CONTROLLER_RECOVERY_MS?0:CONTROLLER_RECOVERY_MS-elapsed;
}
static inline bool controller_recovery_due(controller_recovery_t *r,bool live,uint64_t now)
{
    if(!live){r->watching=false;return false;}
    if(!r->watching){r->watching=true;r->live_since_ms=now;}
    return controller_recovery_remaining(r,now)==0;
}

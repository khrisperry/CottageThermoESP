#pragma once
#include <stdbool.h>
#include <stdint.h>

#define ROOM_LAN_RECOVERY_GRACE_MS 60000ULL

/* Pure policy, evaluated by short, independent read-only status probes.
 * No loop or sleep inside a request, so UI/sensor tasks remain responsive.
 * The data is owned by the controller IO mutex. */
typedef struct {
    uint64_t first_failure_ms;
    uint32_t deferred_probes;
    bool expiry_reported;
} room_lan_recovery_t;

static inline void room_lan_recovery_clear(room_lan_recovery_t *s)
{
    s->first_failure_ms=0;
    s->deferred_probes=0;
    s->expiry_reported=false;
}
static inline bool room_lan_recovery_should_use_radio(room_lan_recovery_t *s,
    uint64_t now_ms,bool wifi_ready,bool already_on_radio)
{
    /* No room Wi-Fi at all: preserve existing immediate safety fallback.
     * Once radio has been selected, continue to use it on LAN probe misses. */
    if(!wifi_ready || already_on_radio)return true;
    if(!s->first_failure_ms)s->first_failure_ms=now_ms?now_ms:1;
    if(now_ms>=s->first_failure_ms &&
       now_ms-s->first_failure_ms>=ROOM_LAN_RECOVERY_GRACE_MS)return true;
    if(s->deferred_probes<UINT32_MAX)++s->deferred_probes;
    return false;
}
static inline uint64_t room_lan_recovery_remaining_ms(
    const room_lan_recovery_t *s,uint64_t now_ms)
{
    if(!s->first_failure_ms)return 0;
    uint64_t elapsed=now_ms>=s->first_failure_ms?
        now_ms-s->first_failure_ms:0;
    return elapsed>=ROOM_LAN_RECOVERY_GRACE_MS?
        0:ROOM_LAN_RECOVERY_GRACE_MS-elapsed;
}

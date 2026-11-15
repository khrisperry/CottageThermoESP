#include <assert.h>
#include <stdint.h>
#include "room_lan_recovery.h"

int main(void)
{
    room_lan_recovery_t state={0};
    /* Initial LAN failure does not immediately trigger a radio switch. */
    assert(!room_lan_recovery_should_use_radio(&state,1000,true,false));
    assert(state.first_failure_ms==1000);
    assert(state.deferred_probes==1);
    assert(room_lan_recovery_remaining_ms(&state,1000)==60000);
    /* Short transient timeouts recover within the 60-second budget. */
    assert(!room_lan_recovery_should_use_radio(&state,59999,true,false));
    assert(room_lan_recovery_remaining_ms(&state,59999)==1001);
    assert(!room_lan_recovery_should_use_radio(&state,60999,true,false));
    assert(room_lan_recovery_should_use_radio(&state,61000,true,false));
    assert(room_lan_recovery_remaining_ms(&state,61000)==0);
    /* Successful LAN handshake clears grace across independent probes. */
    room_lan_recovery_clear(&state);
    assert(state.first_failure_ms==0 && state.deferred_probes==0);
    assert(!room_lan_recovery_should_use_radio(&state,70000,true,false));
    assert(state.first_failure_ms==70000);
    /* Immediate radio fallback if room Wi-Fi is really disconnected. */
    room_lan_recovery_clear(&state);
    assert(room_lan_recovery_should_use_radio(&state,90000,false,false));
    assert(state.first_failure_ms==0);
    /* Existing ESP-NOW selection should not be blocked by the grace policy. */
    assert(room_lan_recovery_should_use_radio(&state,91000,true,true));
    assert(state.first_failure_ms==0);
    /* At uptime 0, retain a nonzero sentinel. */
    room_lan_recovery_clear(&state);
    assert(!room_lan_recovery_should_use_radio(&state,0,true,false));
    assert(room_lan_recovery_remaining_ms(&state,0)==60000);
    return 0;
}

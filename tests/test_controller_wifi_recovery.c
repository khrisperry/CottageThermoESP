#include <assert.h>
#include <stdio.h>
#include "../controller/main/controller_wifi_recovery.h"
int main(void)
{
    assert(!controller_wifi_recovery_needed(0,false));
    assert(!controller_wifi_recovery_needed(1,false));
    assert(!controller_wifi_recovery_needed(2,false));
    assert(controller_wifi_recovery_needed(3,false));
    assert(controller_wifi_recovery_needed(4,false));
    /* Never replace configured offline radio fallback with auto AP recovery. */
    assert(!controller_wifi_recovery_needed(3,true));
    assert(!controller_wifi_recovery_needed(100,true));
    puts("PASS: unattended AP recovery is gated on no paired ESP-NOW");
    return 0;
}

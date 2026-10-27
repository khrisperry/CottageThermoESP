#pragma once
#include <stdbool.h>

/* Enter AP-priority recovery only when no paired ESP-NOW fallback needs
 * channel rendezvous. Paired systems retain normal reconnect behavior and
 * can request recovery with the Super Mini BOOT button after startup. */
static inline bool controller_wifi_recovery_needed(unsigned failed_attempts, bool paired_radio)
{
    return failed_attempts >= 3U && !paired_radio;
}

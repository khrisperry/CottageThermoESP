#pragma once
/* HW-747 Super Mini, the blue channel of the separate WS2812 RGB pixel
 * on GPIO48 (next to the main chip). The SMALL BLUE LED BESIDE USB-C is a
 * BATTERY CHARGE indicator, not software controlled, and often blinks
 * continuously when no battery is connected. The small RED LED beside USB
 * shares GPIO48 and can change when WS2812 data is sent. Neither of these
 * small USB-side LEDs represents controller link health.
 * This pure policy is host-testable and never participates in control flow.
 * Priorities: dangerous fault > newest authenticated transport > offline. */
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CONTROLLER_LED_OFF=0,
    CONTROLLER_LED_ESPNOW=1,
    CONTROLLER_LED_ERROR=2,
} controller_led_mode_t;

/* HTTP and ESP-NOW timestamps are controller monotonic milliseconds.
 * A link expires after 20 seconds without an authenticated transaction.
 * Grace after startup avoids an alarming solid blue during AP/STA setup. */
#define CONTROLLER_LED_LINK_TTL_MS 20000ULL
#define CONTROLLER_LED_BOOT_GRACE_MS 60000ULL
#define CONTROLLER_LED_BLINK_MS 500ULL

static inline bool controller_led_recent(uint64_t now_ms,uint64_t at_ms)
{
    return at_ms && now_ms>=at_ms &&
           now_ms-at_ms<=CONTROLLER_LED_LINK_TTL_MS;
}
static inline controller_led_mode_t controller_led_choose(
    bool fault,bool wifi_up,uint64_t http_at_ms,uint64_t radio_at_ms,
    uint64_t now_ms)
{
    if(fault)return CONTROLLER_LED_ERROR;
    const bool http=wifi_up && controller_led_recent(now_ms,http_at_ms);
    const bool radio=controller_led_recent(now_ms,radio_at_ms);
    if(radio && (!http || radio_at_ms>http_at_ms))return CONTROLLER_LED_ESPNOW;
    if(http)return CONTROLLER_LED_OFF;
    if(now_ms<CONTROLLER_LED_BOOT_GRACE_MS)return CONTROLLER_LED_OFF;
    return CONTROLLER_LED_ERROR;
}
static inline bool controller_led_illuminated(controller_led_mode_t mode,
                                              uint64_t now_ms)
{
    if(mode==CONTROLLER_LED_ERROR)return true;
    if(mode==CONTROLLER_LED_ESPNOW)
        return ((now_ms/CONTROLLER_LED_BLINK_MS)&1ULL)==0;
    return false;
}

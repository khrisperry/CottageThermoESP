#pragma once
/* Pure GPIO48 WS2812 status policy, host-testable.
 * USB-side charge/power LEDs are independent hardware indicators.
 * Advisory only: must never alter servo or thermostat control. */
#include <stdbool.h>
#include <stdint.h>
typedef enum {
    CONTROLLER_LED_OFF=0,      /* Healthy LAN - dark */
    CONTROLLER_LED_ESPNOW=1,   /* ESP-NOW is freshest paired link */
    CONTROLLER_LED_FAULT=2,    /* Live/PWM fault */
    CONTROLLER_LED_BOOT=3,     /* Initial link acquisition */
    CONTROLLER_LED_OFFLINE=4,  /* Room link expired after boot grace */
    CONTROLLER_LED_CONNECTED=5 /* Brief LAN recovery confirmation */
} controller_led_mode_t;
typedef struct { uint8_t red,green,blue; } controller_led_rgb_t;
#define CONTROLLER_LED_LINK_TTL_MS 20000ULL
#define CONTROLLER_LED_BOOT_GRACE_MS 60000ULL
#define CONTROLLER_LED_BLINK_MS 500ULL
#define CONTROLLER_LED_CONNECTED_MS 2000ULL
#define CONTROLLER_LED_PULSE_MS 1600ULL

static inline bool controller_led_recent(uint64_t now_ms,uint64_t at_ms)
{
    return at_ms && now_ms>=at_ms &&
           now_ms-at_ms<=CONTROLLER_LED_LINK_TTL_MS;
}
static inline controller_led_mode_t controller_led_choose(
    bool fault,bool wifi_up,uint64_t http_at_ms,uint64_t radio_at_ms,
    uint64_t now_ms)
{
    if(fault)return CONTROLLER_LED_FAULT;
    const bool http=wifi_up && controller_led_recent(now_ms,http_at_ms);
    const bool radio=controller_led_recent(now_ms,radio_at_ms);
    if(radio && (!http || radio_at_ms>http_at_ms))return CONTROLLER_LED_ESPNOW;
    if(http)return CONTROLLER_LED_OFF;
    if(now_ms<CONTROLLER_LED_BOOT_GRACE_MS)return CONTROLLER_LED_BOOT;
    return CONTROLLER_LED_OFFLINE;
}
static inline controller_led_rgb_t controller_led_color(controller_led_mode_t mode,
                                                        uint64_t now_ms)
{
    controller_led_rgb_t c={0,0,0};
    switch(mode){
        case CONTROLLER_LED_BOOT:{
            uint32_t p=(uint32_t)(now_ms%CONTROLLER_LED_PULSE_MS);
            if(p>CONTROLLER_LED_PULSE_MS/2)p=CONTROLLER_LED_PULSE_MS-p;
            /* Dim purple triangle-wave pulse (1.6-second period). */
            c.red=(uint8_t)(3+12*p/(CONTROLLER_LED_PULSE_MS/2));
            c.blue=(uint8_t)(4+14*p/(CONTROLLER_LED_PULSE_MS/2));
            break;
        }
        case CONTROLLER_LED_CONNECTED:c.green=20;break;
        case CONTROLLER_LED_ESPNOW:
            if(((now_ms/CONTROLLER_LED_BLINK_MS)&1ULL)==0){c.red=20;c.green=8;}
            break;
        case CONTROLLER_LED_OFFLINE:
            if(((now_ms/CONTROLLER_LED_BLINK_MS)&1ULL)==0)c.red=20;
            break;
        case CONTROLLER_LED_FAULT:c.red=20;break;
        case CONTROLLER_LED_OFF:default:break;
    }
    return c;
}
static inline bool controller_led_rgb_equal(controller_led_rgb_t a,
                                            controller_led_rgb_t b)
{
    return a.red==b.red && a.green==b.green && a.blue==b.blue;
}

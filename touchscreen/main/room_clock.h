#pragma once
/* Display-only 12-hour cottage clock. Receives wall time from existing
 * ESP-IDF SNTP clock; no Home Assistant, network or controller dependency.
 * America/Detroit TZ (EST5EDT) is configured at boot in touchscreen main. */
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define ROOM_CLOCK_IDLE_US (60LL*1000000LL)
#define ROOM_CLOCK_VALID_UNIX ((time_t)1735689600) /* 2025-01-01 UTC */

static inline bool room_clock_time_valid(time_t wall)
{
    return wall>=ROOM_CLOCK_VALID_UNIX;
}
static inline bool room_clock_idle(int64_t now_us,int64_t last_touch_us)
{
    return last_touch_us>0 && now_us>=last_touch_us &&
           now_us-last_touch_us>=ROOM_CLOCK_IDLE_US;
}
/* A leading hour digit is blank (-1) for 1..9. Each rendered digit is
 * 0..9 or -1 (blank); unlike LVGL text fonts these have exact segment shape. */
static inline void room_clock_digits(const struct tm *t,int out[4],bool *pm)
{
    int hour=t->tm_hour%12;
    if(hour==0)hour=12;
    out[0]=hour>=10?hour/10:-1;
    out[1]=hour%10;
    out[2]=t->tm_min/10;
    out[3]=t->tm_min%10;
    if(pm)*pm=t->tm_hour>=12;
}

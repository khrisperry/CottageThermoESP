#pragma once
/* Primary room sensor offset, in signed tenths of a Fahrenheit degree.
 * Keep this calibration independent of setpoint and dial calibration.
 * 0 is the default for existing installations. */
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define ROOM_TEMP_OFFSET_MIN_TENTHS (-100)
#define ROOM_TEMP_OFFSET_MAX_TENTHS 100

static inline bool room_temp_offset_valid(int tenths)
{
    return tenths>=ROOM_TEMP_OFFSET_MIN_TENTHS &&
           tenths<=ROOM_TEMP_OFFSET_MAX_TENTHS;
}
static inline float room_temp_correct(float raw_f,int offset_tenths)
{
    return raw_f+(float)offset_tenths/10.0f;
}

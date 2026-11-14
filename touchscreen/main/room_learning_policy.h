#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* Read-only advisory model. None of these values authorize furnace motion. */
#define RL_SAMPLE_INTERVAL_MS 300000ULL
#define RL_MIN_SETTLED_MS (90ULL*60ULL*1000ULL)
#define RL_MAX_RECENT_SPREAD_F 1.5f
#define RL_BINS 21

/* Quarter-step buckets represent normal fractional thermostat positions.
 * Raw exact commanded values always remain in CSV and in mean_commanded_level. */
static inline int rl_quarter_step_bin(float level)
{
    if (!isfinite(level) || level < 1.0f || level > 6.0f) return -1;
    int index=(int)lroundf((level-1.0f)*4.0f);
    if (index<0 || index>=RL_BINS ||
        fabsf(level-(1.0f+0.25f*index))>0.126f) return -1;
    return index;
}
static inline bool rl_settled(uint64_t now_ms,uint64_t from_ms,
                               unsigned consecutive,float spread_f)
{
    return from_ms>0 && now_ms>=from_ms &&
        now_ms-from_ms>=RL_MIN_SETTLED_MS &&
        consecutive>=6 && isfinite(spread_f) &&
        spread_f<=RL_MAX_RECENT_SPREAD_F;
}

#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/* Read-only advisory model. None of these values authorize furnace motion. */
#define RL_SAMPLE_INTERVAL_MS 300000ULL
#define RL_MIN_SETTLED_MS (90ULL*60ULL*1000ULL)
#define RL_MAX_RECENT_SPREAD_F 1.5f
#define RL_BINS 11

/* Only 1.0, 1.5, ... 6.0 are reported as provisional per-position averages.
 * Fractional setpoints between those values are retained in CSV unchanged. */
static inline int rl_half_step_bin(float level)
{
    if (!isfinite(level) || level < 0.9f || level > 6.1f) return -1;
    int index=(int)lroundf((level-1.0f)*2.0f);
    if (index<0 || index>=RL_BINS ||
        fabsf(level-(1.0f+0.5f*index))>0.10f) return -1;
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

#pragma once
#include <stdbool.h>
#include "link_protocol.h"

/* An intentional setpoint / Away request always needs a newly verified
 * controller lease. High-frequency primary sensor reports can use a valid
 * cached lease. This read-only preflight is safe on either transport. */
static inline bool room_link_needs_status(unsigned operation,bool cached_fresh)
{
    return operation==TL_TARGET || operation==TL_AWAY || !cached_fresh;
}

/* A missing or ambiguous mutation response is NOT evidence the command
 * failed at the controller. Never automatically replay it. */
static inline bool room_link_may_replay_mutation(unsigned operation)
{
    (void)operation;
    return false;
}

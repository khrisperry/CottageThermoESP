#pragma once
#include <stdbool.h>
#include "controller_setup.h"

/* Relative angular error below which the physical AS5600 is considered
 * consistent with the saved mechanical calibration. Angular wrap is handled.
 * Do not command motion based on raw encoder values without verification. */
#define CONTROLLER_LIVE_ANGLE_TOLERANCE_DEG 12.0f

/* Requires a complete monotonic encoder calibration for dial 1..6, and
 * recovers the CURRENT fractional dial level from a valid magnetic angle.
 * Returns false if the encoder could describe multiple positions, cannot be
 * related to the calibration, or is outside the measured 1..6 travel. */
bool controller_live_recover(const controller_setup_t *setup,float angle_deg,float *level);

/* Validate a measured encoder angle against the EXPECTED dial command.
 * Used on Live entry and after every servo move. */
bool controller_live_matches(const controller_setup_t *setup,float level,float angle_deg);

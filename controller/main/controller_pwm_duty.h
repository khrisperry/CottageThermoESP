#pragma once

#include <stdint.h>
#include "driver/ledc.h"

/* Set a servo PWM duty on an already-configured LEDC channel.
 * Unlike ledc_set_duty_and_update(), this does not require fade service.
 * ledc_update_duty() also re-enables output after ledc_stop().
 * Caller owns channel configuration, validated travel limits and safety.
 */
esp_err_t controller_pwm_duty_update(ledc_mode_t mode, ledc_channel_t channel, uint32_t duty);

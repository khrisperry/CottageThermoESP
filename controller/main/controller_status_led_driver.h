#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Hardware-layer LED indicator only. Never required for thermostat operation:
 * an LED/RMT initialization error is logged and the controller runs anyway. */
esp_err_t controller_status_led_init(void);
esp_err_t controller_status_led_write(bool blue_on);

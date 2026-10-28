#pragma once
#include "esp_err.h"
#include "controller_status_led.h"
/* RMT status output only. Any failure is nonfatal to thermostat operation. */
esp_err_t controller_status_led_init(void);
esp_err_t controller_status_led_write(controller_led_rgb_t color);

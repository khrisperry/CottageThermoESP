#pragma once
#include <stdint.h>

typedef int esp_err_t;
typedef int ledc_mode_t;
typedef int ledc_channel_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0

/* Deliberately do not declare ledc_set_duty_and_update: using the
 * fade-service-dependent API must break host compilation. */
esp_err_t ledc_set_duty(ledc_mode_t mode,ledc_channel_t channel,uint32_t duty);
esp_err_t ledc_update_duty(ledc_mode_t mode,ledc_channel_t channel);

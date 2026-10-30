#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    bool online, magnet_detected, too_weak, too_strong;
    uint16_t raw_angle;
    uint64_t at_ms;
    esp_err_t error;
} controller_encoder_sample_t;
void controller_encoder_start(void);
controller_encoder_sample_t controller_encoder_snapshot(void);

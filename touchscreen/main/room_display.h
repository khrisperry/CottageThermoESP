#pragma once
#include "driver/i2c_master.h"
void room_display_start(void);
/* Native 4B probe shares the board I2C bus, never repurposes LCD GPIOs. */
i2c_master_bus_handle_t room_display_probe_bus(int sda, int scl);


/* A single sensor sample is corrected before it reaches the controller.
 * Preserve raw Fahrenheit alongside the corrected display/HA reading for
 * calibration diagnostics; no extra controller network requests. */
void room_display_probe_sample(float corrected_f,float raw_f);

#include <stdbool.h>
#include "cJSON.h"
cJSON *room_display_status(void);
bool room_display_camera_fill(void);
void room_display_set_camera_fill(bool fill);

/* Read-only local snapshot for MQTT; never polls or changes the controller. */
cJSON *room_display_mqtt_state(void);

/* Shared target-only command queue for touchscreen and HA; no raw servo control. */
bool room_display_request_target(float target_f);
/* Freeze Hold (50F) / Home is a controller-persisted climate preset. */
bool room_display_request_away(bool enabled);

#pragma once
#include <stddef.h>
#include "cJSON.h"
#include "esp_err.h"
void room_ha_start(void);
cJSON *room_ha_status(void); // Caller owns the sanitized snapshot.
cJSON *room_ha_command(cJSON *payload,int *http_status);
esp_err_t room_ha_camera(void *buffer,size_t capacity,int *http_status,int *length);
#define ROOM_HA_CAMERA_CAPACITY (256*1024+2)

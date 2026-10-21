#pragma once
#include <stdbool.h>
#include "cJSON.h"
void room_controller_start(void);
cJSON *room_controller_status(void);
cJSON *room_controller_target(float target,int *http_status);
bool room_controller_sample(float temperature);
bool room_controller_fallback(void);
bool room_controller_radio_ready(void);
bool room_controller_radio_online(void);

int room_controller_mqtt_state(void);
const char *room_controller_sync_state(void);

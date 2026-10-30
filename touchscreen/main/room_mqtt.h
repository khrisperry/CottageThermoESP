#pragma once
#include <stdbool.h>
#include "cJSON.h"
void room_mqtt_start(void);
int room_mqtt_state(void); /* 0 disabled, 1 connecting/offline, 2 broker connected */
cJSON *room_mqtt_status(void);
bool room_mqtt_reannounce(void);

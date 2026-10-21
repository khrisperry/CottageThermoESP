#pragma once
#include <stdbool.h>
#include "cJSON.h"
void controller_mqtt_start(void);
/* Implemented by controller main, serialized with its engine/config lock. */
bool controller_climate_command(const char *key,float target,const char *value);
cJSON *controller_climate_state(void);

bool controller_mqtt_configured(void);
bool controller_mqtt_online(void);

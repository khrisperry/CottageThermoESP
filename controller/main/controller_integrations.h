#pragma once
#include "integration_config.h"
void controller_integrations_init(void);
const cottage_integration_t *controller_integrations_get(void);
bool controller_integrations_save(cJSON *payload,bool *changed);

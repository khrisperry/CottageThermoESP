#pragma once
#include <stdbool.h>
#include "esp_http_server.h"
#include "cJSON.h"
bool room_ota_busy(void);
void room_ota_register(httpd_handle_t server);
void room_ota_confirm_boot(void);
cJSON *room_ota_status(void);

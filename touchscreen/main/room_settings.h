#pragma once
#include "esp_http_server.h"
void room_settings_init(void);
void room_settings_register(httpd_handle_t server);
const char *room_settings_ha_url(void);
const char *room_settings_ha_token(void);
const char *room_settings_controller_url(void);

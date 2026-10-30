#pragma once
#include <stdbool.h>
#include "esp_http_server.h"
#include "cJSON.h"
#include "integration_config.h"
void room_settings_init(void);
void room_settings_register(httpd_handle_t server);
const char *room_settings_ha_url(void);
const char *room_settings_ha_token(void);
const char *room_settings_controller_url(void);
const char *room_settings_ship_url(void);
bool room_settings_ships_enabled(void);
unsigned room_settings_ship_range(void);
bool room_web_authorized(httpd_req_t *request,bool mutation);
bool room_settings_reboot_later(void);
/* Only radio pairing is forwarded to the controller. MQTT credentials stay here. */
cJSON *room_settings_controller_integrations(void);
const cottage_integration_t *room_settings_integrations(void);
bool room_settings_radio_managed(void);
/* Local room probe calibration, bounded to +/-10.0 F in 0.1 F steps.
 * Independent NVS key: preserve older profile blob and all credentials.
 * Reading the offset is lock-free and safe from the sensor task. */
int room_settings_temp_offset_tenths(void);
float room_settings_temp_offset_f(void);
/* Independent persisted monitor configuration. Default: disabled, bench. */
unsigned room_settings_furnace_config(void);

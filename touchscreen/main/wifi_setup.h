#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_http_server.h"
void room_wifi_start(void);
bool room_wifi_connected(void);
void room_wifi_register(httpd_handle_t server);
void room_wifi_request_setup(void);
bool room_wifi_setup_request(httpd_req_t *request);
esp_err_t room_wifi_setup_get(httpd_req_t *request);
bool room_wifi_setup_hint(char *buffer,size_t size);

#pragma once
#include "cJSON.h"
#include "esp_err.h"
#include "esp_http_server.h"

/* Passive 5-minute paired observations, never a control or calibration write. */
void room_learning_start(void);
void room_learning_probe(float corrected_f,float raw_f);
cJSON *room_learning_status(void);
esp_err_t room_learning_csv(httpd_req_t *request);

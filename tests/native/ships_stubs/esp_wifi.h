#pragma once
#include "host.h"
#define WIFI_IF_STA 0
typedef struct {int8_t rssi;} wifi_ap_record_t;
esp_err_t esp_wifi_get_mac(int,uint8_t*);
esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*);

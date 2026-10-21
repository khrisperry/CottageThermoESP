#pragma once
#include "host.h"
#define ESP_ERR_TIMEOUT -7
#define OTA_WITH_SEQUENTIAL_WRITES ((size_t)-2)
typedef unsigned esp_ota_handle_t;
typedef int esp_ota_img_states_t;
enum {ESP_OTA_IMG_PENDING_VERIFY=1,ESP_OTA_IMG_VALID=2};
typedef struct {const char *label;size_t size;} esp_partition_t;
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const void*);
int esp_ota_get_state_partition(const esp_partition_t*,int*);
int esp_ota_mark_app_valid_cancel_rollback(void);
int esp_ota_begin(const esp_partition_t*,size_t,unsigned*);
int esp_ota_write(unsigned,const void*,size_t);
int esp_ota_end(unsigned);
int esp_ota_abort(unsigned);
int esp_ota_set_boot_partition(const esp_partition_t*);

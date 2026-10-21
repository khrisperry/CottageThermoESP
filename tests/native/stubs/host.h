#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "lvgl.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_STATE -3
#define ESP_ERR_INVALID_ARG -4
#define ESP_ERROR_CHECK(x) assert((x)==0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
#define MALLOC_CAP_INTERNAL 4
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define CONFIG_ROOM_NATIVE_DISPLAY 1
#define CONFIG_ROOM_DISPLAY_ROTATE_180 0
#define CONFIG_ROOM_HA_BRIDGE_URL "http://ha.test"
#define CONFIG_ROOM_CONTROLLER_URL "http://controller.test"
#define CONFIG_ROOM_HA_BRIDGE_TOKEN "test-bridge-token"
#define CONFIG_ROOM_API_TOKEN "0123456789abcdef0123456789abcdef"
#define BSP_I2C_SDA 47
#define BSP_I2C_SCL 48
#define portMAX_DELAY 0
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
typedef void *i2c_master_bus_handle_t;
typedef void *SemaphoreHandle_t;
typedef void *QueueHandle_t;
int64_t esp_timer_get_time(void);
uint32_t esp_random(void);
int xSemaphoreTake(void*,int);
int xSemaphoreGive(void*);
void *xSemaphoreCreateMutex(void);
void *xQueueCreate(int,int);
int xQueueSend(void*,const void*,int);
int xQueueReceive(void*,void*,int);
int uxQueueMessagesWaiting(void*);
int xTaskCreate(void(*)(void*),const char*,int,void*,int,void*);
void vTaskDelete(void*);
void vTaskDelay(int);
void *heap_caps_malloc(size_t,int);
bool bsp_display_lock(uint32_t);
void bsp_display_unlock(void);
lv_display_t *bsp_display_start(void);
void bsp_display_rotate(lv_display_t*,lv_display_rotation_t);
int bsp_display_brightness_set(int);
void *bsp_i2c_get_handle(void);

size_t heap_caps_get_free_size(int);
size_t heap_caps_get_largest_free_block(int);

void esp_fill_random(void*,size_t);

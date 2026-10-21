#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
typedef void *QueueHandle_t;
typedef void *esp_codec_dev_handle_t;
typedef struct {uint8_t bits_per_sample,channel;uint16_t channel_mask;uint32_t sample_rate;int mclk_multiple;} esp_codec_dev_sample_info_t;
#ifndef CONFIG_ROOM_NATIVE_DISPLAY
#define CONFIG_ROOM_NATIVE_DISPLAY 1
#endif
#define ESP_OK 0
#define ESP_CODEC_DEV_OK 0
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY -1
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGI(...) audio_test_log(__VA_ARGS__)
#define ESP_LOGW(...) audio_test_log(__VA_ARGS__)
void audio_test_log(const char *,const char *,...);
int64_t esp_timer_get_time(void);
esp_err_t nvs_open(const char*,int,nvs_handle_t*);
esp_err_t nvs_get_u8(nvs_handle_t,const char*,uint8_t*);
esp_err_t nvs_set_u8(nvs_handle_t,const char*,uint8_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueReceive(QueueHandle_t,void*,int);
int xQueueSend(QueueHandle_t,const void*,int);
void vQueueDelete(QueueHandle_t);
void vTaskDelay(int);
int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,void*);
esp_codec_dev_handle_t bsp_audio_codec_speaker_init(void);
esp_err_t bsp_audio_poweramp_enable(bool);
int esp_codec_dev_open(esp_codec_dev_handle_t,esp_codec_dev_sample_info_t*);
int esp_codec_dev_close(esp_codec_dev_handle_t);
void esp_codec_dev_delete(esp_codec_dev_handle_t);
int esp_codec_dev_write(esp_codec_dev_handle_t,void*,int);
int esp_codec_dev_set_out_vol(esp_codec_dev_handle_t,int);
int esp_codec_dev_set_out_mute(esp_codec_dev_handle_t,bool);

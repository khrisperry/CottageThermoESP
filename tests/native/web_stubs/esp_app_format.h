#pragma once
#include <stdint.h>
#define ESP_IMAGE_HEADER_MAGIC 0xE9
#define ESP_CHIP_ID_ESP32S3 9
typedef struct __attribute__((packed)) {uint8_t magic,segment_count,spi_mode,spi_speed_size;uint32_t entry_addr;uint8_t pins[4];uint16_t chip_id;uint8_t rest[10];} esp_image_header_t;
typedef struct {uint32_t address,size;} esp_image_segment_header_t;

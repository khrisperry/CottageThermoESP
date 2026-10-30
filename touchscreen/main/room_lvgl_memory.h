#pragma once
#include <stddef.h>
#include "esp_heap_caps.h"
#include "esp_err.h"
/* One bounded LVGL TLSF arena in PSRAM keeps internal RAM for RGB DMA/Wi-Fi. */
static inline void *room_lvgl_pool_alloc(size_t size)
{
    void *pool=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(pool?ESP_OK:ESP_ERR_NO_MEM);return pool;
}

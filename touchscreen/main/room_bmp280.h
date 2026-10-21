#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
/* BMP280/BME280 test-only temperature reader. Shares the BSP bus; never resets the bus. */
typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    uint16_t t1;
    int16_t t2,t3;
    unsigned discovery_attempts;
} room_bmp280_t;
bool room_bmp280_read(room_bmp280_t *sensor,float *fahrenheit);

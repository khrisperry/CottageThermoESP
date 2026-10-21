#include "room_bmp280.h"
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool reg_read(room_bmp280_t *s,uint8_t reg,uint8_t *out,unsigned n)
{return i2c_master_transmit_receive(s->device,&reg,1,out,n,200)==ESP_OK;}
static bool reg_write(room_bmp280_t *s,uint8_t reg,uint8_t value)
{uint8_t data[]={reg,value};return i2c_master_transmit(s->device,data,sizeof(data),200)==ESP_OK;}
static void release(room_bmp280_t *s)
{if(s->device){i2c_master_bus_rm_device(s->device);s->device=NULL;}}
static bool wait_ready(room_bmp280_t *s,uint8_t mask)
{
    for(unsigned n=0;n<20;++n){
        uint8_t status;
        if(!reg_read(s,0xf3,&status,1))return false;
        if(!(status&mask))return true;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return false;
}
static uint16_t le16(const uint8_t *p){return (uint16_t)(p[0]|((uint16_t)p[1]<<8));}
static int16_t signed_le16(const uint8_t *p)
{uint16_t v=le16(p);return (int16_t)(v>=32768?(int32_t)v-65536:v);}
static bool discover(room_bmp280_t *s)
{
    if(!s->bus)return false;
    for(unsigned address=0x76;address<=0x77;++address){
        i2c_device_config_t cfg={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=address,.scl_speed_hz=100000};
        if(i2c_master_bus_add_device(s->bus,&cfg,&s->device)!=ESP_OK){s->device=NULL;continue;}
        uint8_t id=0;
        /* Do not reset unknown devices, including a BME280 (ID 0x60). */
        if(!reg_read(s,0xd0,&id,1)||id!=0x58){release(s);continue;}
        if(!reg_write(s,0xe0,0xb6)){release(s);continue;}
        vTaskDelay(pdMS_TO_TICKS(5));
        uint8_t calibration[6];
        if(!wait_ready(s,1)||!reg_read(s,0x88,calibration,sizeof(calibration))){release(s);continue;}
        s->t1=le16(calibration);s->t2=signed_le16(calibration+2);s->t3=signed_le16(calibration+4);
        if(!s->t1||s->t1==UINT16_MAX||!s->t2){release(s);continue;}
        ESP_LOGI("room_bmp280","BMP280 test sensor found at 0x%02x",address);
        return true;
    }
    ESP_LOGW("room_bmp280","No valid BMP280 at 0x76/0x77; check 3.3V, SDA47/SCL48 and CS/SDO");
    return false;
}
bool room_bmp280_read(room_bmp280_t *s,float *fahrenheit)
{
    if(!s||!fahrenheit||(!s->device&&!discover(s)))return false;
    /* Forced conversion, temperature x1, pressure disabled. No continuous self-heating. */
    if(!reg_write(s,0xf4,0x21)){release(s);return false;}
    vTaskDelay(pdMS_TO_TICKS(5));
    uint8_t raw[3];
    if(!wait_ready(s,8)||!reg_read(s,0xfa,raw,sizeof(raw))){release(s);return false;}
    uint32_t adc=((uint32_t)raw[0]<<12)|((uint32_t)raw[1]<<4)|(raw[2]>>4);
    if(adc==0x80000){release(s);return false;} /* Disabled/unmeasured sentinel. */
    /* Bosch's floating-point temperature compensation, using factory calibration. */
    double var1=((double)adc/16384.0-(double)s->t1/1024.0)*s->t2;
    double delta=(double)adc/131072.0-(double)s->t1/8192.0;
    double c=(var1+delta*delta*s->t3)/5120.0;
    float f=(float)(c*1.8+32.0);
    if(!isfinite(f)||f<32||f>110)return false;
    *fahrenheit=f;return true;
}

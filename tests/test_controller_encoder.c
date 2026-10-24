/* Execute the actual polling task against a scripted I2C device. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "encoder_host.h"
#include "../controller/main/controller_encoder.h"
static jmp_buf stop;
static void (*task)(void *);
static uint8_t reply[3];
static int read_error;
const char *esp_err_to_name(esp_err_t err) {(void)err;return "test";}
int64_t esp_timer_get_time(void) {return 123000;}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {return (void *)1;}
int xSemaphoreTake(SemaphoreHandle_t m,int timeout) {(void)m;(void)timeout;return 1;}
int xSemaphoreGive(SemaphoreHandle_t m) {(void)m;return 1;}
int xTaskCreate(void (*f)(void *),const char *n,unsigned stack,void *a,unsigned p,void *h)
{(void)n;(void)stack;(void)a;(void)p;(void)h;task=f;return pdPASS;}
void vTaskDelay(int ticks) {assert(ticks==500);longjmp(stop,1);}
void test_log(const char *tag,const char *fmt,...) {(void)tag;(void)fmt;}
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *c,i2c_master_bus_handle_t *b)
{assert(c->sda_io_num==6 && c->scl_io_num==7);*b=(void *)1;return ESP_OK;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t b,const i2c_device_config_t *c,i2c_master_dev_handle_t *d)
{(void)b;assert(c->device_address==0x36 && c->scl_speed_hz==100000);*d=(void *)2;return ESP_OK;}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d,const void *tx,size_t tn,void *rx,size_t rn,int timeout)
{assert(d==(void *)2 && tn==1 && *(const uint8_t *)tx==0x0b && rn==3 && timeout==100);memcpy(rx,reply,3);return read_error;}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t d) {(void)d;return 0;}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t b) {(void)b;return 0;}
static controller_encoder_sample_t read_once(uint8_t status,uint16_t angle,int err)
{reply[0]=status;reply[1]=angle>>8;reply[2]=angle;read_error=err;if(!setjmp(stop))task(NULL);return controller_encoder_snapshot();}
int main(void)
{
 assert(!controller_encoder_snapshot().online);
 controller_encoder_start();assert(task);
 controller_encoder_sample_t s=read_once(0x20,0xf800,0);
 assert(s.online && s.magnet_detected && !s.too_weak && !s.too_strong && s.raw_angle==2048 && s.at_ms==123);
 s=read_once(0x30,0,0);assert(s.magnet_detected && s.too_weak);
 s=read_once(0x28,4095,0);assert(s.too_strong && s.raw_angle==4095);
 s=read_once(0,0,0);assert(s.online && !s.magnet_detected);
 s=read_once(0x20,2048,99);assert(!s.online && !s.magnet_detected && s.at_ms==0 && s.error==99);
 s=read_once(0x20,1024,0);assert(s.online && s.raw_angle==1024);
 puts("AS5600 polling: decode, magnet flags, failure invalidation and recovery passed");
}

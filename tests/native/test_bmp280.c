/* Exercise production driver with register-level fixtures. No network/hardware. */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/room_bmp280.c"
static unsigned address=0x76,id=0x58,adc=519888,adds,removes,resets,conversions,wait_ms;
static bool stuck_reset,stuck_measurement,read_failure,add_failure,bad_cal;
void probe_test_log(const char *tag,const char *format,...){(void)tag;(void)format;}
void vTaskDelay(unsigned ticks){wait_ms+=ticks;}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,const i2c_device_config_t *cfg,i2c_master_dev_handle_t *dev)
{
 assert(bus==(void*)1&&cfg->scl_speed_hz==100000);++adds;
 if(add_failure)return -1;
 *dev=(void*)(uintptr_t)cfg->device_address;return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev){assert(dev);++removes;return ESP_OK;}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev,const void *command,size_t cmd_len,void *out,size_t n,int timeout)
{
 assert(cmd_len==1&&timeout==200);
 if((uintptr_t)dev!=address)return -1;
 uint8_t reg=*(const uint8_t*)command,*bytes=out;
 if(reg==0xd0){assert(n==1);*bytes=id;}
 else if(reg==0xf3){assert(n==1);*bytes=(stuck_reset?1:0)|(stuck_measurement?8:0);}
 else if(reg==0x88){
  assert(n==6);uint8_t cal[]={0x70,0x6b,0x43,0x67,0x18,0xfc}; /* 27504,26435,-1000 */
  memcpy(bytes,cal,n);if(bad_cal)memset(bytes,0,n);
 }
 else if(reg==0xfa){assert(n==3);if(read_failure)return -1;bytes[0]=adc>>12;bytes[1]=adc>>4;bytes[2]=(adc&15)<<4;}
 else assert(false);
 return ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev,const void *data,size_t n,int timeout)
{
 assert((uintptr_t)dev==address&&id==0x58&&n==2&&timeout==200);
 const uint8_t *p=data;
 if(p[0]==0xe0){assert(p[1]==0xb6);++resets;}
 else {assert(p[0]==0xf4&&p[1]==0x21);++conversions;}
 return ESP_OK;
}
static void drop(room_bmp280_t *s){release(s);}
int main(void)
{
 room_bmp280_t s={.bus=(void*)1};float f=-99;
 assert(!room_bmp280_read(NULL,&f));assert(!room_bmp280_read(&s,NULL));
 assert(room_bmp280_read(&s,&f));assert(fabsf(f-77.148f)<0.01f);assert(resets==1&&conversions==1);
 unsigned old_adds=adds;assert(room_bmp280_read(&s,&f));assert(adds==old_adds&&resets==1);drop(&s);
 address=0x77;assert(room_bmp280_read(&s,&f));assert((uintptr_t)s.device==0x77);drop(&s);
 unsigned old_resets=resets;id=0x60;assert(!room_bmp280_read(&s,&f));assert(!s.device&&resets==old_resets);id=0x58;
 bad_cal=true;assert(!room_bmp280_read(&s,&f));assert(!s.device);bad_cal=false;
 stuck_reset=true;unsigned before=wait_ms;assert(!room_bmp280_read(&s,&f));assert(wait_ms-before<=110&&!s.device);stuck_reset=false;
 stuck_measurement=true;assert(!room_bmp280_read(&s,&f));assert(!s.device);stuck_measurement=false;
 assert(room_bmp280_read(&s,&f));read_failure=true;assert(!room_bmp280_read(&s,&f));assert(!s.device);read_failure=false;
 assert(room_bmp280_read(&s,&f));adc=0x80000;assert(!room_bmp280_read(&s,&f));assert(!s.device);adc=519888;
 assert(room_bmp280_read(&s,&f));adc=0;assert(!room_bmp280_read(&s,&f));adc=519888;drop(&s);
 add_failure=true;assert(!room_bmp280_read(&s,&f));assert(!s.device);add_failure=false;
 assert(room_bmp280_read(&s,&f));drop(&s);
 assert(removes>0);
 puts("PASS: BMP280 0x76/0x77 discovery, chip ID, factory compensation, forced conversion, bounded waits, failures and reconnect");
 return 0;
}

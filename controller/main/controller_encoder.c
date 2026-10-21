/* AS5600 bench telemetry only: never writes configuration or commands motion.
 * Register map: ams OSRAM AS5600 DS000365, STATUS 0x0b / RAW ANGLE 0x0c.
 */
#include "controller_encoder.h"
#include "sdkconfig.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG="controller_encoder";
static SemaphoreHandle_t mutex;
static controller_encoder_sample_t sample={.error=ESP_ERR_INVALID_STATE};
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t device;

controller_encoder_sample_t controller_encoder_snapshot(void)
{
    controller_encoder_sample_t result={.error=ESP_ERR_INVALID_STATE};
    if(mutex) {
        xSemaphoreTake(mutex,portMAX_DELAY);
        result=sample;
        xSemaphoreGive(mutex);
    }
    return result;
}
static void poll(void *arg)
{
    (void)arg;
    unsigned count=0;
    bool was_online=false;
    for(;;) {
        uint8_t reg=0x0b, data[3]={0};
        esp_err_t err=i2c_master_transmit_receive(device,&reg,1,data,sizeof(data),100);
        controller_encoder_sample_t next={.error=err};
        if(err==ESP_OK) {
            next.online=true;
            next.at_ms=esp_timer_get_time()/1000;
            next.magnet_detected=(data[0]&0x20)!=0;
            next.too_weak=(data[0]&0x10)!=0;
            next.too_strong=(data[0]&0x08)!=0;
            next.raw_angle=((data[1]&0x0f)<<8)|data[2];
        }
        xSemaphoreTake(mutex,portMAX_DELAY);
        sample=next;
        xSemaphoreGive(mutex);
        if(next.online!=was_online || count++%10==0) {
            if(next.online) ESP_LOGI(TAG,"AS5600 raw=%u angle=%.2f deg magnet=%s weak=%d strong=%d",
                next.raw_angle,next.raw_angle*360.0/4096.0,next.magnet_detected?"detected":"missing",
                next.too_weak,next.too_strong);
            else ESP_LOGW(TAG,"AS5600 read failed: %s (SDA GPIO%d / SCL GPIO%d)",
                esp_err_to_name(err),CONFIG_THERMO_ENCODER_SDA,CONFIG_THERMO_ENCODER_SCL);
        }
        was_online=next.online;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
void controller_encoder_start(void)
{
    mutex=xSemaphoreCreateMutex();
    if(!mutex) { ESP_LOGE(TAG,"AS5600 mutex allocation failed"); return; }
    if(CONFIG_THERMO_ENCODER_SDA==CONFIG_THERMO_ENCODER_SCL ||
       CONFIG_THERMO_ENCODER_SDA==CONFIG_THERMO_SERVO_GPIO ||
       CONFIG_THERMO_ENCODER_SCL==CONFIG_THERMO_SERVO_GPIO) {
        sample.error=ESP_ERR_INVALID_ARG;
        ESP_LOGE(TAG,"AS5600 pins conflict with each other or servo output"); return;
    }
    i2c_master_bus_config_t cfg={.i2c_port=I2C_NUM_0,
        .sda_io_num=CONFIG_THERMO_ENCODER_SDA,.scl_io_num=CONFIG_THERMO_ENCODER_SCL,
        .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,
        .flags.enable_internal_pullup=true};
    esp_err_t err=i2c_new_master_bus(&cfg,&bus);
    if(err==ESP_OK) {
        i2c_device_config_t dev={.dev_addr_length=I2C_ADDR_BIT_LEN_7,
            .device_address=0x36,.scl_speed_hz=100000};
        err=i2c_master_bus_add_device(bus,&dev,&device);
    }
    if(err==ESP_OK && xTaskCreate(poll,"as5600",3072,NULL,3,NULL)!=pdPASS)
        err=ESP_ERR_NO_MEM;
    if(err!=ESP_OK) {
        sample.error=err;
        ESP_LOGE(TAG,"AS5600 startup failed: %s",esp_err_to_name(err));
        if(device) { i2c_master_bus_rm_device(device); device=NULL; }
        if(bus) { i2c_del_master_bus(bus); bus=NULL; }
    } else ESP_LOGI(TAG,"Read-only AS5600 0x36: SDA GPIO%d / SCL GPIO%d",
        CONFIG_THERMO_ENCODER_SDA,CONFIG_THERMO_ENCODER_SCL);
}

/* Independent room client with native Waveshare 4B and browser panels. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include "room_audio.h"
#include "room_display.h"
#include "room_transport.h"
#include "room_ha.h"
#include "wifi_setup.h"
#include "room_settings.h"
#include "room_temp_cal.h"
#include "room_bmp280.h"
#include "room_controller.h"
#include "room_ships.h"
#include "room_ota.h"
#include "room_mqtt.h"
#include "esp_heap_caps.h"

static const char *TAG="room_thermostat";
static i2c_master_dev_handle_t probe;
static room_bmp280_t bmp280;
static uint8_t crc(const uint8_t *p)
{
    uint8_t c=0xff;
    for(int i=0;i<2;++i){c^=p[i];for(int b=0;b<8;++b)c=c&0x80?(c<<1)^0x31:c<<1;}
    return c;
}
static bool read_probe(float *f)
{
    if(CONFIG_ROOM_SENSOR==3)return room_bmp280_read(&bmp280,f);
    if(!probe)return false;
    uint8_t cmd31[]={0x24,0x00},cmd4=0xfd,raw[6];
    const uint8_t *cmd=CONFIG_ROOM_SENSOR==1?cmd31:&cmd4;
    if(i2c_master_transmit(probe,cmd,CONFIG_ROOM_SENSOR==1?2:1,200)!=ESP_OK)return false;
    vTaskDelay(pdMS_TO_TICKS(20));
    if(i2c_master_receive(probe,raw,sizeof(raw),200)!=ESP_OK || crc(raw)!=raw[2] || crc(raw+3)!=raw[5])return false;
    float c=-45.0f+175.0f*((raw[0]<<8)|raw[1])/65535.0f;
    *f=c*1.8f+32.0f;return isfinite(*f)&&*f>=32&&*f<=110;
}
static void sensor_task(void *arg)
{
    for(;;) {
        float f;
        if(read_probe(&f)) {
            const float raw_f=f;
            const int offset=room_settings_temp_offset_tenths();
            f=room_temp_correct(raw_f,offset);
            /* The controller's protocol intentionally admits 32..110 F.
             * Never send an out-of-range corrected value, but report why. */
            if(!isfinite(f)||f<32.0f||f>110.0f){
                ESP_LOGW(TAG,
                    "Primary probe raw %.2f F plus offset %+.1f F is outside 32..110 F; no sample sent",
                    raw_f,offset/10.0f);
            }else{
                /* One correction before the reading branches to display,
                 * MQTT, and controller. No offset is added to targets. */
                room_display_probe_sample(f,raw_f);
                if(CONFIG_ROOM_SENSOR==3){
                    ESP_LOGI(TAG,"BMP/BME280 TEST raw %.2f F + offset %+.1f F = %.2f F; not sent to controller",
                             raw_f,offset/10.0f,f);
                }else{
                    bool sent=room_controller_sample(f);
                    ESP_LOGI(TAG,
                        "%s raw %.2f F + offset %+.1f F = room %.2f F; report %s",
                        CONFIG_ROOM_SENSOR==2?"SHT45/SHT4x":"SHT31",
                        raw_f,offset/10.0f,f,sent?"acknowledged":"not acknowledged");
                }
            }
        } else ESP_LOGW(TAG,"No fresh valid probe sample; no temperature report sent");
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}
static esp_err_t settings_redirect(httpd_req_t *r)
{
    httpd_resp_set_status(r,"303 See Other");httpd_resp_set_hdr(r,"Location","/settings");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,"Open device settings at /settings");
}
static esp_err_t root(httpd_req_t *r)
{
    if(room_wifi_setup_request(r))return room_wifi_setup_get(r);
    return settings_redirect(r);
}
static void *json_allocate(size_t size){return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_ROOM_NATIVE_DISPLAY
    cJSON_Hooks hooks={.malloc_fn=json_allocate,.free_fn=free};cJSON_InitHooks(&hooks);
#endif
    room_settings_init();
    /* Digital clock follows Michigan local time including automatic DST.
     * This only affects localtime_r() formatting. Existing SNTP source,
     * Wi-Fi/ESP-NOW, controller protocol and MQTT are unchanged. */
    if(setenv("TZ","EST5EDT,M3.2.0/2,M11.1.0/2",1)==0)tzset();
    else ESP_LOGW(TAG,"Cannot set local clock timezone; time may show UTC");
    room_wifi_start();
    esp_err_t mdns_err=mdns_init();
    if(mdns_err==ESP_OK)ESP_LOGI(TAG,"Controller mDNS lookup available; searching CottageThermoController.local");
    else ESP_LOGW(TAG,"mDNS discovery unavailable: %s; explicit controller URL / ESP-NOW remain usable",esp_err_to_name(mdns_err));
    room_controller_start();
    room_ha_start();
    room_ships_start();
    room_display_start();
    room_audio_start();
    room_mqtt_start();
    if(CONFIG_ROOM_SENSOR && CONFIG_ROOM_SDA>=0 && CONFIG_ROOM_SCL>=0 && CONFIG_ROOM_SDA!=CONFIG_ROOM_SCL) {
        i2c_master_bus_handle_t bus=NULL;
#if CONFIG_ROOM_NATIVE_DISPLAY
        bus=room_display_probe_bus(CONFIG_ROOM_SDA,CONFIG_ROOM_SCL);
#else
        i2c_master_bus_config_t cfg={.i2c_port=I2C_NUM_0,.sda_io_num=CONFIG_ROOM_SDA,.scl_io_num=CONFIG_ROOM_SCL,
            .clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true};
        ESP_ERROR_CHECK(i2c_new_master_bus(&cfg,&bus));
#endif
        i2c_device_config_t dev={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=0x44,.scl_speed_hz=100000};
        if(CONFIG_ROOM_SENSOR==3)bmp280.bus=bus;
        else if(bus){
            ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&dev,&probe));
            ESP_LOGI(TAG,"%s primary probe configured: address 0x44, SDA%d/SCL%d; samples every 15 seconds",
                CONFIG_ROOM_SENSOR==2?"SHT45/SHT4x":"SHT31",CONFIG_ROOM_SDA,CONFIG_ROOM_SCL);
        }
    }
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=12288;cfg.max_uri_handlers=24;httpd_handle_t h;ESP_ERROR_CHECK(httpd_start(&h,&cfg));
    httpd_uri_t routes[]={ {.uri="/",.method=HTTP_GET,.handler=root},
        {.uri="/home",.method=HTTP_GET,.handler=settings_redirect}};
    for(unsigned i=0;i<2;++i)ESP_ERROR_CHECK(httpd_register_uri_handler(h,&routes[i]));
    room_wifi_register(h);
    room_settings_register(h);
    room_ota_register(h);
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();
    if((CONFIG_ROOM_SENSOR==3&&bmp280.bus)||probe)
        ESP_ERROR_CHECK(xTaskCreate(sensor_task,"room_probe",6144,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    else ESP_LOGW(TAG,"Local probe disabled: configure probe type and pins");
    room_ota_confirm_boot();
    ESP_LOGI(TAG,"Room client v0.15.3; native display and browser services ready");
}


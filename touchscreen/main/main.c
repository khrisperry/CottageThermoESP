/* Independent room client with native Waveshare 4B and browser panels. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver/i2c_master.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include "ha_panel.h"
#include "room_display.h"
#include "room_transport.h"
#include "room_ha.h"
#include "wifi_setup.h"

static const char *TAG="room_thermostat";
static i2c_master_dev_handle_t probe;
extern const unsigned char page_start[] asm("_binary_room_html_start");
extern const unsigned char page_end[] asm("_binary_room_html_end");
// Each HTTP client is owned by its caller. Shared controller settings live upstream only.
static esp_err_t exchange(const char *path,const char *payload,char *out,size_t cap,int *status)
{
    int length;
    return room_http(CONFIG_ROOM_CONTROLLER_URL,"X-Thermo-Token",CONFIG_ROOM_API_TOKEN,
                     path,payload,out,cap,3000,status,&length);
}
static uint8_t crc(const uint8_t *p)
{
    uint8_t c=0xff;
    for(int i=0;i<2;++i){c^=p[i];for(int b=0;b<8;++b)c=c&0x80?(c<<1)^0x31:c<<1;}
    return c;
}
static bool read_probe(float *f)
{
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
            char payload[128],response[256];int status=0;
            // Timestamp immediately after measurement; do not queue or replay failed samples.
            int64_t sampled=esp_timer_get_time();
            snprintf(payload,sizeof(payload),"{\"source\":\"room\",\"temp_f\":%.3f,\"age_ms\":%lld}",
                f,(long long)((esp_timer_get_time()-sampled)/1000));
            esp_err_t err=exchange("/api/temperature",payload,response,sizeof(response),&status);
            ESP_LOGI(TAG,"Room %.2f F; report %s HTTP %d",f,esp_err_to_name(err),status);
        } else ESP_LOGW(TAG,"No fresh valid probe sample; no temperature report sent");
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}
static esp_err_t root(httpd_req_t *r)
{
    if(room_wifi_setup_request(r))return room_wifi_setup_get(r);
    httpd_resp_set_type(r,"text/html; charset=utf-8");return httpd_resp_send(r,(const char*)page_start,page_end-page_start-1);
}
static bool browser_authorized(httpd_req_t *r)
{
    char t[48]={0};
    return strlen(CONFIG_ROOM_API_TOKEN)==32 && httpd_req_get_hdr_value_str(r,"X-Thermo-Token",t,sizeof(t))==ESP_OK && !strcmp(t,CONFIG_ROOM_API_TOKEN);
}
static esp_err_t proxy(httpd_req_t *r)
{
    if(!browser_authorized(r))return httpd_resp_send_err(r,HTTPD_401_UNAUTHORIZED,"Pairing token required");
    char payload[256],out[2048];const char *post=NULL;int status=0;
    if(r->method==HTTP_POST) {
        if(r->content_len<=0 || r->content_len>=sizeof(payload))return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Invalid body");
        int at=0;while(at<r->content_len){int n=httpd_req_recv(r,payload+at,r->content_len-at);if(n<=0)return ESP_FAIL;at+=n;}payload[at]=0;
        cJSON *j=cJSON_ParseWithLengthOpts(payload,strlen(payload)+1,NULL,true);double target=0;cJSON *v=cJSON_GetObjectItemCaseSensitive(j,"target_f");
        bool valid=cJSON_IsObject(j)&&cJSON_GetArraySize(j)==1&&cJSON_IsNumber(v)&&isfinite(v->valuedouble);
        if(valid)target=v->valuedouble;
        cJSON_Delete(j);
        if(!valid || target<45 || target>80)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Only target_f 45..80 is accepted");
        // Room client can change target, never arm/synchronize/enable a controller mode.
        snprintf(payload,sizeof(payload),"{\"target_f\":%.2f}",target);post=payload;
    }
    esp_err_t err=exchange(r->method==HTTP_POST?"/api/config":"/api/status",post,out,sizeof(out),&status);
    if(err!=ESP_OK) {
        httpd_resp_set_status(r,"503 Service Unavailable");
        httpd_resp_set_type(r,"text/plain");
        return httpd_resp_sendstr(r,"Controller unavailable; no command queued");
    }
    if(status!=200)return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"Controller rejected request; check pairing and settings");
    httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");return httpd_resp_sendstr(r,out);
}
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());
    room_wifi_start();
    room_ha_start();
    room_display_start();
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
        if(bus)ESP_ERROR_CHECK(i2c_master_bus_add_device(bus,&dev,&probe));
    }
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=8192;cfg.max_uri_handlers=16;httpd_handle_t h;ESP_ERROR_CHECK(httpd_start(&h,&cfg));
    httpd_uri_t routes[]={ {.uri="/",.method=HTTP_GET,.handler=root},
        {.uri="/api/status",.method=HTTP_GET,.handler=proxy}, {.uri="/api/target",.method=HTTP_POST,.handler=proxy}};
    for(unsigned i=0;i<3;++i)ESP_ERROR_CHECK(httpd_register_uri_handler(h,&routes[i]));
    ha_panel_register(h);
    room_wifi_register(h);
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();
    if(probe && strlen(CONFIG_ROOM_API_TOKEN)==32)xTaskCreate(sensor_task,"room_probe",6144,NULL,5,NULL);
    else ESP_LOGW(TAG,"Probe reporting disabled: configure probe pins/type and 32-character pairing token");
    ESP_LOGI(TAG,"Room client v0.7.0; native display and browser services ready");
}

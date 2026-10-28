/* Non-blocking controller status indication: WS2812's blue channel only.
 * HW-747 ESP32-S3 Super Mini typically connects an RGB pixel to GPIO48.
 * GPIO48 LED is different from a C3 Super Mini's GPIO8 binary blue LED.
 * No PWM, settings, networking or safety decision is changed here. */
#include "controller_status_led.h"
#include "controller_status_led_driver.h"
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LED_GPIO 48
#define LED_RMT_HZ 10000000
#define LED_BLUE_LEVEL 18 /* Modest brightness for an enclosure at night */

static const char *TAG="controller_status_led";
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;

esp_err_t controller_status_led_init(void)
{
    const rmt_tx_channel_config_t cfg={
        .clk_src=RMT_CLK_SRC_DEFAULT,
        .gpio_num=LED_GPIO,
        .mem_block_symbols=64,
        .resolution_hz=LED_RMT_HZ,
        .trans_queue_depth=2
    };
    esp_err_t err=rmt_new_tx_channel(&cfg,&channel);
    if(err!=ESP_OK)return err;
    const rmt_copy_encoder_config_t encoder_cfg={0};
    err=rmt_new_copy_encoder(&encoder_cfg,&encoder);
    if(err==ESP_OK)err=rmt_enable(channel);
    if(err!=ESP_OK){
        if(encoder){rmt_del_encoder(encoder);encoder=NULL;}
        rmt_del_channel(channel);channel=NULL;
        return err;
    }
    ESP_LOGI(TAG,"HW-747 status RGB LED configured on GPIO%d, blue only",LED_GPIO);
    return ESP_OK;
}
esp_err_t controller_status_led_write(bool blue_on)
{
    if(!channel||!encoder)return ESP_ERR_INVALID_STATE;
    /* WS2812 GRB format: green=0 red=0 blue=18 or 0.
     * Each bit is 1.2 us; last symbol holds the line low for 60 us.
     * Fixed stack buffer is valid until rmt_tx_wait_all_done returns. */
    rmt_symbol_word_t sym[25];
    const uint32_t pixel=blue_on?LED_BLUE_LEVEL:0;
    for(int bit=23;bit>=0;--bit){
        const bool one=(pixel>>bit)&1U;
        sym[23-bit]=(rmt_symbol_word_t){
            .level0=1,.duration0=one?9:3,
            .level1=0,.duration1=one?3:9
        };
    }
    sym[24]=(rmt_symbol_word_t){
        .level0=0,.duration0=300,.level1=0,.duration1=300
    };
    rmt_transmit_config_t tx={.loop_count=0};
    esp_err_t err=rmt_transmit(channel,encoder,sym,sizeof(sym),&tx);
    if(err==ESP_OK)err=rmt_tx_wait_all_done(channel,50);
    return err;
}

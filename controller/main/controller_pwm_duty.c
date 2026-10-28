#include "controller_pwm_duty.h"

esp_err_t controller_pwm_duty_update(ledc_mode_t mode, ledc_channel_t channel, uint32_t duty)
{
    /* The firmware never uses LEDC fades. The combined
     * ledc_set_duty_and_update() API checks the fade service even for a
     * normal duty update in ESP-IDF v6.0.1, returning ESP_FAIL unless the
     * service was installed. These ordinary duty APIs do not require it. */
    esp_err_t err=ledc_set_duty(mode,channel,duty);
    if(err!=ESP_OK)return err;
    return ledc_update_duty(mode,channel);
}

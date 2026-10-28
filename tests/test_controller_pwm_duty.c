/* Host-executable regression for the ACTUAL production PWM duty wrapper.
 * ESP-IDF v6.0.1 ledc_set_duty_and_update unexpectedly needs a fade service;
 * controller PWM intentionally does not install or use fade. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "controller_pwm_duty.h"

static int set_calls,update_calls;
static bool signal_on;
static uint32_t requested_duty,active_duty;
static bool fail_set,fail_update;

esp_err_t ledc_set_duty(ledc_mode_t mode,ledc_channel_t channel,uint32_t duty)
{
    assert(mode==LEDC_LOW_SPEED_MODE && channel==LEDC_CHANNEL_0);
    ++set_calls;
    if(fail_set)return ESP_FAIL;
    requested_duty=duty;
    return ESP_OK;
}

esp_err_t ledc_update_duty(ledc_mode_t mode,ledc_channel_t channel)
{
    assert(mode==LEDC_LOW_SPEED_MODE && channel==LEDC_CHANNEL_0);
    ++update_calls;
    if(fail_update)return ESP_FAIL;
    active_duty=requested_duty;
    signal_on=true; /* Actual ESP-IDF _ledc_update_duty() enables sig_out. */
    return ESP_OK;
}

static void fake_ledc_stop(void){signal_on=false;}

int main(void)
{
    /* First update: ordinary duty without a fade-service dependency. */
    assert(controller_pwm_duty_update(0,0,614)==ESP_OK);
    assert(signal_on && active_duty==614 && set_calls==1 && update_calls==1);
    /* Subsequent servo movement updates the duty. */
    assert(controller_pwm_duty_update(0,0,716)==ESP_OK);
    assert(signal_on && active_duty==716 && set_calls==2 && update_calls==2);
    /* Servo Stop must disable PWM; a later Test may re-enable it. */
    fake_ledc_stop();assert(!signal_on);
    assert(controller_pwm_duty_update(0,0,512)==ESP_OK);
    assert(signal_on && active_duty==512 && set_calls==3 && update_calls==3);
    /* A failed set must not call update. A failed update must propagate. */
    fail_set=true;
    assert(controller_pwm_duty_update(0,0,820)==ESP_FAIL);
    assert(set_calls==4 && update_calls==3 && active_duty==512);
    fail_set=false;fail_update=true;
    assert(controller_pwm_duty_update(0,0,821)==ESP_FAIL);
    assert(set_calls==5 && update_calls==4 && active_duty==512);
    puts("PASS: non-fade LEDC duty update, repeated movements, Stop/restart and error propagation");
    return 0;
}

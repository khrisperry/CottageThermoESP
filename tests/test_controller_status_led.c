#include <assert.h>
#include <stdio.h>
#include "controller_status_led.h"
int main(void)
{
    assert(controller_led_choose(false,false,0,0,1000)==CONTROLLER_LED_OFF);
    assert(controller_led_choose(false,false,0,0,59000)==CONTROLLER_LED_OFF);
    assert(controller_led_choose(false,false,0,0,60000)==CONTROLLER_LED_ERROR);
    assert(controller_led_choose(false,true,98000,0,100000)==CONTROLLER_LED_OFF);
    assert(controller_led_choose(false,true,98000,99000,100000)==CONTROLLER_LED_ESPNOW);
    assert(controller_led_choose(false,true,99000,98000,100000)==CONTROLLER_LED_OFF);
    assert(controller_led_choose(false,false,99000,99500,100000)==CONTROLLER_LED_ESPNOW);
    assert(controller_led_choose(false,true,30000,31000,100000)==CONTROLLER_LED_ERROR);
    assert(controller_led_choose(true,true,99999,100000,100000)==CONTROLLER_LED_ERROR);
    assert(controller_led_choose(false,true,99999,0,100000)==CONTROLLER_LED_OFF);
    assert(!controller_led_illuminated(CONTROLLER_LED_OFF,0));
    assert(!controller_led_illuminated(CONTROLLER_LED_OFF,1000));
    assert(controller_led_illuminated(CONTROLLER_LED_ERROR,1000));
    assert(controller_led_illuminated(CONTROLLER_LED_ESPNOW,0));
    assert(!controller_led_illuminated(CONTROLLER_LED_ESPNOW,500));
    assert(controller_led_illuminated(CONTROLLER_LED_ESPNOW,1000));
    assert(!controller_led_illuminated(CONTROLLER_LED_ESPNOW,1500));
    puts("PASS: no blue on Wi-Fi, blue flash on ESP-NOW, solid blue on error; fault priority");
    return 0;
}

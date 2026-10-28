#include <assert.h>
#include <stdio.h>
#include "controller_status_led.h"
static void rgb(controller_led_rgb_t c,unsigned r,unsigned g,unsigned b)
{assert(c.red==r&&c.green==g&&c.blue==b);}
int main(void)
{
    assert(controller_led_choose(false,false,0,0,1000)==CONTROLLER_LED_BOOT);
    assert(controller_led_choose(false,false,0,0,59000)==CONTROLLER_LED_BOOT);
    assert(controller_led_choose(false,false,0,0,60000)==CONTROLLER_LED_OFFLINE);
    assert(controller_led_choose(false,true,98000,0,100000)==CONTROLLER_LED_OFF);
    assert(controller_led_choose(false,true,98000,99000,100000)==CONTROLLER_LED_ESPNOW);
    assert(controller_led_choose(false,true,99000,98000,100000)==CONTROLLER_LED_OFF);
    assert(controller_led_choose(false,false,99000,99500,100000)==CONTROLLER_LED_ESPNOW);
    assert(controller_led_choose(false,true,30000,31000,100000)==CONTROLLER_LED_OFFLINE);
    assert(controller_led_choose(true,true,99999,100000,100000)==CONTROLLER_LED_FAULT);
    assert(controller_led_choose(false,true,99999,0,100000)==CONTROLLER_LED_OFF);
    rgb(controller_led_color(CONTROLLER_LED_OFF,1),0,0,0);
    rgb(controller_led_color(CONTROLLER_LED_CONNECTED,1),0,20,0);
    rgb(controller_led_color(CONTROLLER_LED_ESPNOW,0),20,8,0);
    rgb(controller_led_color(CONTROLLER_LED_ESPNOW,500),0,0,0);
    rgb(controller_led_color(CONTROLLER_LED_OFFLINE,0),20,0,0);
    rgb(controller_led_color(CONTROLLER_LED_OFFLINE,500),0,0,0);
    rgb(controller_led_color(CONTROLLER_LED_FAULT,500),20,0,0);
    controller_led_rgb_t boot=controller_led_color(CONTROLLER_LED_BOOT,800);
    assert(boot.red>0&&boot.green==0&&boot.blue>0);
    assert(boot.red>controller_led_color(CONTROLLER_LED_BOOT,0).red);
    assert(controller_led_rgb_equal(controller_led_color(CONTROLLER_LED_BOOT,0),
                                    controller_led_color(CONTROLLER_LED_BOOT,1600)));
    puts("PASS: boot purple, LAN green-then-off, ESP-NOW amber, offline flashing red, fault solid red");
    return 0;
}

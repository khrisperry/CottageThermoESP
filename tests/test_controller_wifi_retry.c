#include <assert.h>
#include <stdio.h>
#include "../controller/main/controller_wifi_retry.h"
int main(void)
{
    controller_wifi_retry_t s={0};
    assert(controller_wifi_retry_tick(&s,1000,true)==CONTROLLER_WIFI_CONNECT);
    assert(controller_wifi_retry_tick(&s,5999,true)==CONTROLLER_WIFI_WAIT);
    assert(controller_wifi_retry_tick(&s,6000,true)==CONTROLLER_WIFI_ABORT);
    /* No disconnect event: state is already reset and a retry still occurs. */
    assert(!s.connecting&&s.due==26000);
    for(uint32_t n=6250;n<26000;n+=250)assert(controller_wifi_retry_tick(&s,n,true)==CONTROLLER_WIFI_WAIT);
    assert(controller_wifi_retry_tick(&s,26000,true)==CONTROLLER_WIFI_CONNECT);
    assert(controller_wifi_retry_tick(&s,31000,true)==CONTROLLER_WIFI_ABORT);
    s=(controller_wifi_retry_t){.connecting=true,.started=UINT32_MAX-1000};
    assert(controller_wifi_retry_tick(&s,3999,true)==CONTROLLER_WIFI_ABORT);
    s=(controller_wifi_retry_t){.due=UINT32_MAX-100};
    assert(controller_wifi_retry_tick(&s,100,true)==CONTROLLER_WIFI_CONNECT);
    puts("PASS: controller timeout clears state without event, repeated retries and clock rollover");
}

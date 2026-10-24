#include <assert.h>
#include <stdio.h>
#include "../shared/thermo_link/link_protocol.h"
#include "../touchscreen/main/room_health.h"
int main(void)
{
    tl_guard_t g={0};uint64_t boot=999,now=10000;
    tl_packet_t p=tl_request(TL_STATUS,42,1),decoded;
    assert(tl_check(&g,&p,boot,now)==TL_OK);
    char hex[sizeof(p)*2+1];tl_hex(&p,hex);assert(tl_unhex(hex,&decoded));assert(!memcmp(&p,&decoded,sizeof(p)));
    hex[3]='x';assert(!tl_unhex(hex,&decoded));assert(!tl_unhex("00",&decoded));
    p=tl_request(TL_TARGET,42,2);p.controller_boot=boot;p.deadline_ms=now+4000;p.value=67;
    assert(tl_check(&g,&p,boot,now)==TL_OK);g.sequence=p.sequence;
    /* Shared guard: a duplicate arriving over the other transport is rejected. */
    assert(tl_check(&g,&p,boot,now)==TL_REPLAY);
    ++p.sequence;assert(tl_check(&g,&p,boot,now)==TL_OK);
    assert(tl_check(&g,&p,boot+1,now)==TL_EXPIRED); /* controller reboot */
    assert(tl_check(&g,&p,boot,now+5000)==TL_EXPIRED);
    p.deadline_ms=now+5001;assert(tl_check(&g,&p,boot,now)==TL_EXPIRED);
    p.deadline_ms=now+4000;p.value=NAN;assert(tl_check(&g,&p,boot,now)==TL_INVALID);
    p.value=81;assert(tl_check(&g,&p,boot,now)==TL_INVALID);
    p.operation=TL_SAMPLE;p.value=65;p.age_ms=2001;assert(tl_check(&g,&p,boot,now)==TL_INVALID);
    p.age_ms=1000;assert(tl_check(&g,&p,boot,now)==TL_OK);
    /* A stale hello cannot reset a current client's sequence or enroll a new
     * panel during the previous panel's command lease. */
    p=tl_request(TL_STATUS,42,1);assert(tl_check(&g,&p,boot,now)==TL_OK);assert(g.sequence==2);
    p.client=43;assert(tl_check(&g,&p,boot,now)==TL_REPLAY);
    assert(tl_check(&g,&p,boot,now+5001)==TL_OK);
    p.reserved[0]=1;assert(!tl_valid(&p));p.reserved[0]=0;p.version++;assert(!tl_valid(&p));
    room_health_t h={.wifi=true,.ha=true,.controller=true};
    assert(!strcmp(room_health_status(h).text,"Online"));
    h.ship_configured=true;assert(!strcmp(room_health_status(h).text,"Ship Tracking Offline"));h.ship_online=true;
    h.ha=false;assert(!strcmp(room_health_status(h).text,"HA Offline"));
    h.radio=true;assert(!strcmp(room_health_status(h).text,"ESPNOW"));
    h.controller=false;assert(!strcmp(room_health_status(h).text,"Offline"));
    h.ha=true;assert(!strcmp(room_health_status(h).text,"Heat Offline"));
    puts("PASS: packet validation, shared replay guard, expiry/reboot, samples and status priority");
}

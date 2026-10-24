#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool wifi,ha,controller,radio,local_sensor,ha_configured,radio_configured,ship_configured,ship_online; } room_health_t;
typedef struct {const char *text;uint32_t color;unsigned other_faults;} room_health_status_t;
static inline room_health_status_t room_health_status(room_health_t h)
{
    unsigned faults=(!h.wifi)+(!h.ha)+(!h.controller)+(h.ship_configured&&!h.ship_online);
    room_health_status_t s={"Online",0x64dfc4,0};
    if(!h.controller&&!h.ha){s.text="Offline";s.color=0xff6464;}
    else if(!h.controller){s.text="Heat Offline";s.color=0xff6464;}
    else if(!h.ha){s.text="HA Offline";s.color=h.ha_configured?0xff6464:0xf5cc65;}
    else if(h.ship_configured&&!h.ship_online){s.text="Ship Tracking Offline";s.color=0xff6464;}
    /* Confirmed fallback takes priority over HA loss, never over total loss. */
    if(h.radio&&h.controller){s.text="ESPNOW";s.color=0xffa24c;}
    s.other_faults=faults>1?faults-1:0;return s;
}

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "controller_live.h"

static controller_setup_t calibrated(void)
{
    controller_setup_t s=controller_setup_defaults();
    s.start_us=500;s.end_us=2500;
    s.angle_mask=0x7e;s.temperature_mask=63;
    for(int i=0;i<8;i++)s.pulse_us[i]=600+i*250;
    for(int i=0;i<6;i++){
        s.temperature_f[i]=45+i*7;
        s.angle_deg[i+1]=fmodf(320-i*43+360,360);
    }
    assert(!controller_setup_ready(&s));return s;
}
int main(void)
{
    controller_setup_t s=calibrated();float level=0;
    assert(controller_live_recover(&s,320,&level)&&fabsf(level-1)<0.001f);
    assert(controller_live_recover(&s,277,&level)&&fabsf(level-2)<0.001f);
    assert(controller_live_recover(&s,105,&level)&&fabsf(level-6)<0.001f);
    assert(controller_live_recover(&s,62,&level)==false);
    assert(controller_live_recover(&s,298.5f,&level)&&fabsf(level-1.5f)<0.001f);
    assert(controller_live_recover(&s,191.0f,&level)&&fabsf(level-4)<0.001f);
    assert(controller_live_matches(&s,2.5f,255.5f));
    assert(!controller_live_matches(&s,2.5f,212));
    assert(!controller_live_recover(&s,NAN,&level));
    s.angle_mask=0;assert(!controller_live_recover(&s,270,&level));
    s=calibrated();s.angle_deg[4]=s.angle_deg[3];assert(!controller_live_recover(&s,270,&level));
    s=calibrated();s.angle_deg[4]=310;assert(!controller_live_recover(&s,270,&level));
    puts("PASS: AS5600 wrap, interpolated startup recovery, endpoint rejection, encoder mismatch and invalid saved angles");
}

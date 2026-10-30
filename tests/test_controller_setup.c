#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "controller_setup.h"
static controller_setup_t complete(bool reverse)
{
    controller_setup_t s=controller_setup_defaults();s.start_us=500;s.end_us=2500;
    for(int i=0;i<8;i++)s.pulse_us[i]=reverse?2400-i*250:600+i*250;
    s.temperature_mask=63;
    for(int i=0;i<6;i++)s.temperature_f[i]=45+i*7;
    return s;
}
int main(void)
{
    controller_setup_t s=controller_setup_defaults();assert(controller_setup_valid(&s));assert(controller_setup_ready(&s));
    assert(controller_setup_contains(&s,500));assert(!controller_setup_contains(&s,499));assert(!controller_setup_contains(&s,2501));
    s=complete(false);assert(!controller_setup_ready(&s));th_config_t c=th_defaults();assert(controller_setup_map(&s,&c));
    assert(c.map_count==6 && c.map[0].level==1 && c.map[5].level==6);
    int us;assert(!th_map_level(&c,0,&us));assert(!th_map_level(&c,7,&us));assert(th_map_level(&c,3.5,&us)&&us==1475);
    s=complete(true);assert(!controller_setup_ready(&s));assert(controller_setup_map(&s,&c));assert(c.map[0].pulse_us>c.map[5].pulse_us);
    s.pulse_us[4]=s.pulse_us[3];assert(controller_setup_ready(&s));s=complete(false);s.pulse_us[4]=1100;assert(controller_setup_ready(&s));
    s=complete(false);s.temperature_f[2]=s.temperature_f[1];assert(controller_setup_ready(&s));s.temperature_f[2]=NAN;assert(!controller_setup_valid(&s));
    s=complete(false);s.temperature_mask=31;assert(controller_setup_ready(&s));
    s=complete(false);s.start_us=2200;s.end_us=600;assert(!controller_setup_valid(&s));
    s=complete(false);s.start_us=2400;s.end_us=500;assert(controller_setup_valid(&s));assert(controller_setup_contains(&s,600));assert(!controller_setup_contains(&s,2450));
    s.start_us=s.end_us;assert(!controller_setup_valid(&s));
    s=complete(false);s.angle_mask=1;s.angle_deg[0]=360;assert(!controller_setup_valid(&s));s.angle_deg[0]=359.9f;assert(controller_setup_valid(&s));
    s.magic=0;assert(!controller_setup_valid(&s));
    puts("PASS: travel limits, reverse direction, ordered positions, estimated temperatures, full pulse range and Off/Max exclusion");
}

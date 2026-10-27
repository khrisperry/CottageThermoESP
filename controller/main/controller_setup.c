#include "controller_setup.h"
#include <math.h>

controller_setup_t controller_setup_defaults(void)
{
    return (controller_setup_t){.magic=CONTROLLER_SETUP_MAGIC};
}
static bool pulse_valid(int us) { return us>=500 && us<=2500; }
bool controller_setup_contains(const controller_setup_t *s,int us)
{
    if(!pulse_valid(us))return false;
    if(!s->start_us || !s->end_us)return true;
    int lo=s->start_us<s->end_us?s->start_us:s->end_us;
    int hi=s->start_us>s->end_us?s->start_us:s->end_us;
    return us>=lo && us<=hi;
}
bool controller_setup_valid(const controller_setup_t *s)
{
    if(s->magic!=CONTROLLER_SETUP_MAGIC || s->angle_mask>255 || s->temperature_mask>63)return false;
    if((s->start_us && !pulse_valid(s->start_us)) || (s->end_us && !pulse_valid(s->end_us)))return false;
    if(s->start_us && s->end_us && s->start_us==s->end_us)return false;
    for(int i=0;i<8;i++) {
        if(s->pulse_us[i] && !controller_setup_contains(s,s->pulse_us[i]))return false;
        if((s->angle_mask&(1U<<i)) && (!s->pulse_us[i] || !isfinite(s->angle_deg[i]) || s->angle_deg[i]<0 || s->angle_deg[i]>=360))return false;
    }
    for(int i=0;i<6;i++)if((s->temperature_mask&(1U<<i)) &&
        (!isfinite(s->temperature_f[i]) || s->temperature_f[i]<40 || s->temperature_f[i]>90))return false;
    return true;
}
const char *controller_setup_ready(const controller_setup_t *s)
{
    if(!controller_setup_valid(s))return "Calibration is invalid. Reset and capture it again.";
    if(!s->start_us || !s->end_us)return "Capture both travel limits in Calibration.";
    static const char *missing[]={"Capture Off.","Capture dial 1.","Capture dial 2.","Capture dial 3.","Capture dial 4.","Capture dial 5.","Capture dial 6.","Capture Max."};
    int direction=0;
    for(int i=0;i<8;i++) {
        if(!s->pulse_us[i])return missing[i];
        if(i) {
            int d=s->pulse_us[i]>s->pulse_us[i-1]?1:-1;
            if(s->pulse_us[i]==s->pulse_us[i-1] || (direction && d!=direction))return "Dial positions must progress in one direction from Off through Max.";
            direction=d;
        }
    }
    if(s->temperature_mask!=63)return "Enter an estimated temperature for each dial number, 1 through 6.";
    for(int i=1;i<6;i++)if(s->temperature_f[i]<=s->temperature_f[i-1])return "Estimated temperatures must increase from dial 1 through 6.";
    return NULL;
}
bool controller_setup_map(const controller_setup_t *s,th_config_t *c)
{
    if(controller_setup_ready(s))return false;
    th_config_t next=*c;
    next.map_count=6;
    for(int i=0;i<6;i++)next.map[i]=(th_point_t){s->temperature_f[i],i+1,s->pulse_us[i+1]};
    if(!th_config_valid(&next))return false;
    *c=next;return true;
}

#include "controller_live.h"
#include <math.h>
#include <stddef.h>

static float wrap_delta(float degrees)
{
    while(degrees>180.0f)degrees-=360.0f;
    while(degrees<=-180.0f)degrees+=360.0f;
    return degrees;
}
static bool map_angles(const controller_setup_t *s,float mapped[6])
{
    if(!s || controller_setup_ready(s) || (s->angle_mask & 0x7EU)!=0x7EU)return false;
    for(int i=0;i<6;i++) {
        float a=s->angle_deg[i+1];
        if(!isfinite(a)||a<0||a>=360)return false;
        if(i==0)mapped[i]=a;
        else {
            float delta=wrap_delta(a-s->angle_deg[i]);
            /* Each adjacent dial mark must be resolvable and progress in
             * one consistent direction. Prevent ambiguous 360-degree wrap. */
            if(fabsf(delta)<3.0f || fabsf(delta)>105.0f)return false;
            if(i>1 && delta*(mapped[i-1]-mapped[i-2])<=0)return false;
            mapped[i]=mapped[i-1]+delta;
        }
    }
    return fabsf(mapped[5]-mapped[0])<340.0f;
}
static bool expected_angle(const controller_setup_t *s,float level,float *angle)
{
    float a[6];
    if(!map_angles(s,a)||!isfinite(level)||level<1||level>6)return false;
    if(level==6){*angle=a[5];return true;}
    int index=(int)floorf(level)-1;
    if(index<0||index>=5)return false;
    *angle=a[index]+(a[index+1]-a[index])*(level-(index+1));
    return true;
}
bool controller_live_expected(const controller_setup_t *s,float level,float *angle)
{
    float expected;
    if(!angle||!expected_angle(s,level,&expected))return false;
    *angle=fmodf(expected+720.0f,360.0f);
    return true;
}
bool controller_live_matches(const controller_setup_t *s,float level,float angle_deg)
{
    float expected=0;
    return isfinite(angle_deg)&&angle_deg>=0&&angle_deg<360&&
        expected_angle(s,level,&expected)&&
        fabsf(wrap_delta(angle_deg-expected))<=CONTROLLER_LIVE_ANGLE_TOLERANCE_DEG;
}
bool controller_live_angle_near(float verified_deg,float restored_deg)
{
    return isfinite(verified_deg)&&verified_deg>=0&&verified_deg<360 &&
           isfinite(restored_deg)&&restored_deg>=0&&restored_deg<360 &&
           fabsf(wrap_delta(restored_deg-verified_deg))<=CONTROLLER_LIVE_REJOIN_TOLERANCE_DEG;
}
bool controller_live_recover(const controller_setup_t *s,float angle_deg,float *level)
{
    if(!level||!isfinite(angle_deg)||angle_deg<0||angle_deg>=360)return false;
    float a[6];
    if(!map_angles(s,a))return false;
    /* Map the measured circle into the unwrapped 1..6 reference range.
     * A 340-degree maximum means only one turn can plausibly match. */
    float measured=a[0]+wrap_delta(angle_deg-s->angle_deg[1]);
    float lo=fminf(a[0],a[5]),hi=fmaxf(a[0],a[5]);
    while(measured<lo-12)measured+=360;
    while(measured>hi+12)measured-=360;
    if(measured<lo-12||measured>hi+12)return false;
    for(int i=0;i<5;i++){
        float min=fminf(a[i],a[i+1])-0.0001f,max=fmaxf(a[i],a[i+1])+0.0001f;
        if(measured>=min&&measured<=max){
            float recovered=i+1+(measured-a[i])/(a[i+1]-a[i]);
            if(recovered>=1&&recovered<=6&&controller_live_matches(s,recovered,angle_deg)){
                *level=recovered;return true;
            }
        }
    }
    if(fabsf(measured-a[0])<=CONTROLLER_LIVE_ANGLE_TOLERANCE_DEG){*level=1;return true;}
    if(fabsf(measured-a[5])<=CONTROLLER_LIVE_ANGLE_TOLERANCE_DEG){*level=6;return true;}
    return false;
}

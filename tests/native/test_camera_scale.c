#include <assert.h>
#include <stdio.h>
#include "../../touchscreen/main/room_camera_scale.h"
static void check(unsigned w,unsigned h)
{
 unsigned fit=room_camera_scale(w,h,454,306,false),fill=room_camera_scale(w,h,454,306,true);
 assert(w*fit<=454*256U&&h*fit<=306*256U);
 assert(w*fill>=454*256U&&h*fill>=306*256U);
}
int main(void)
{
 assert(room_camera_scale(240,135,454,306,false)>256);
 check(240,135);check(480,270);check(320,480);check(480,320);check(454,306);
 assert(room_camera_scale(454,306,454,306,false)==256);
 assert(room_camera_scale(0,135,454,306,false)==256);
 assert(room_camera_scale(240,135,0,306,true)==256);
 puts("PASS: camera fit/fill preserves aspect ratio, upscales and covers viewport");
 return 0;
}

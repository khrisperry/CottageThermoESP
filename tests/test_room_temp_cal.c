#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "room_temp_cal.h"
static void near(float a,float b){assert(fabsf(a-b)<0.001f);}
int main(void)
{
    assert(room_temp_offset_valid(-100));
    assert(room_temp_offset_valid(0));
    assert(room_temp_offset_valid(100));
    assert(!room_temp_offset_valid(-101));
    assert(!room_temp_offset_valid(101));
    near(room_temp_correct(82.4f,0),82.4f);
    near(room_temp_correct(82.4f,-100),72.4f);
    near(room_temp_correct(82.4f,-25),79.9f);
    near(room_temp_correct(62.8f,35),66.3f);
    near(room_temp_correct(50.0f,100),60.0f);
    puts("PASS: bounded +/-10F offset in 0.1F steps, independent of target");
    return 0;
}

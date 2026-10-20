#pragma once
#include <stdbool.h>
#include <stdint.h>
/* LVGL scale 256 = 1x. Fit retains the full image; fill crops symmetrically. */
static inline uint32_t room_camera_scale(unsigned width,unsigned height,unsigned viewport_width,unsigned viewport_height,bool fill)
{
    if(!width||!height||!viewport_width||!viewport_height)return 256;
    uint32_t x=(viewport_width*256U+(fill?width-1:0))/width;
    uint32_t y=(viewport_height*256U+(fill?height-1:0))/height;
    uint32_t result=fill?(x>y?x:y):(x<y?x:y);
    return result<1?1:result>8192?8192:result;
}

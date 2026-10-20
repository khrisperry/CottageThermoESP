#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum {ROOM_JPEG_OK,ROOM_JPEG_INVALID,ROOM_JPEG_UNSUPPORTED,ROOM_JPEG_TOO_LARGE} room_jpeg_result_t;
typedef struct {unsigned source_width,source_height,width,height,scale;size_t bytes;} room_camera_size_t;
/* Inspect bounded JPEG markers before passing data to TJPGD. Its header-info
 * helper does not guarantee an exact resize, and only baseline JPEG is supported. */
static inline room_jpeg_result_t room_camera_size(const uint8_t *data,size_t length,room_camera_size_t *out)
{
    *out=(room_camera_size_t){0};
    if(length<4||data[0]!=0xff||data[1]!=0xd8)return ROOM_JPEG_INVALID;
    size_t at=2;
    while(at<length){
        if(data[at++]!=0xff)return ROOM_JPEG_INVALID;
        while(at<length&&data[at]==0xff)++at;
        if(at>=length)return ROOM_JPEG_INVALID;
        unsigned marker=data[at++];
        if(marker==0xd9||marker==0xda)return ROOM_JPEG_INVALID;
        if(marker==0x01||(marker>=0xd0&&marker<=0xd7))continue;
        if(length-at<2)return ROOM_JPEG_INVALID;
        size_t segment=((unsigned)data[at]<<8)|data[at+1];
        if(segment<2||segment>length-at)return ROOM_JPEG_INVALID;
        if(marker==0xc2)return ROOM_JPEG_UNSUPPORTED;
        if(marker==0xc0){
            if(segment<8)return ROOM_JPEG_INVALID;
            unsigned height=((unsigned)data[at+3]<<8)|data[at+4];
            unsigned width=((unsigned)data[at+5]<<8)|data[at+6];
            unsigned components=data[at+7];
            if(!width||!height||segment<8+3*components)return ROOM_JPEG_INVALID;
            if(data[at+2]!=8||(components!=1&&components!=3))return ROOM_JPEG_UNSUPPORTED;
            out->source_width=width;out->source_height=height;
            for(unsigned scale=0;scale<4;++scale){
                unsigned w=width>>scale,h=height>>scale;
                if(w&&h&&w<=480&&h<=320){
                    out->scale=scale;out->width=w;out->height=h;out->bytes=(size_t)w*h*2;
                    return ROOM_JPEG_OK;
                }
            }
            return ROOM_JPEG_TOO_LARGE;
        }
        at+=segment;
    }
    return ROOM_JPEG_INVALID;
}

/* No network or decoder required: production JPEG bounds and scaling policy. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../touchscreen/main/room_camera_size.h"
static void header(uint8_t *j,unsigned w,unsigned h)
{
    const uint8_t baseline[]={0xff,0xd8,0xff,0xc0,0,17,8,0,0,0,0,3,1,0x22,0,2,0x11,1,3,0x11,1};
    memcpy(j,baseline,sizeof(baseline));j[7]=h>>8;j[8]=h;j[9]=w>>8;j[10]=w;
}
int main(void)
{
    uint8_t j[21];room_camera_size_t size;
    header(j,480,320);assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_OK&&size.scale==0&&size.bytes==480*320*2);
    header(j,1920,1080);assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_OK&&size.scale==2&&size.width==480&&size.height==270&&size.bytes==480*270*2);
    header(j,3840,2160);assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_OK&&size.scale==3);
    header(j,480,640);assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_OK&&size.scale==1&&size.width==240&&size.height==320);
    header(j,8192,8192);assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_TOO_LARGE);
    header(j,0,320);assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_INVALID);
    header(j,480,320);assert(room_camera_size(j,sizeof(j)-1,&size)==ROOM_JPEG_INVALID);
    j[3]=0xc2;assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_UNSUPPORTED);
    header(j,480,320);j[6]=12;assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_UNSUPPORTED);
    header(j,480,320);j[11]=4;assert(room_camera_size(j,sizeof(j),&size)==ROOM_JPEG_INVALID);
    assert(room_camera_size((uint8_t*)"not a jpeg",10,&size)==ROOM_JPEG_INVALID);
    uint8_t with_app[27]={0xff,0xd8,0xff,0xe0,0,4,0,0};
    header(j,1920,1080);memcpy(with_app+8,j+2,19);
    assert(room_camera_size(with_app,sizeof(with_app),&size)==ROOM_JPEG_OK&&size.scale==2);
    with_app[4]=0x7f;assert(room_camera_size(with_app,sizeof(with_app),&size)==ROOM_JPEG_INVALID);
    puts("PASS: bounded JPEG markers, baseline formats, 1080p/4K downscale, output limits and malformed inputs");
    return 0;
}

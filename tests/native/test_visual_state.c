#include <assert.h>
#include <stdio.h>
#include "../../touchscreen/main/room_visual_state.h"
int main(void)
{
 assert(room_visual_state("Not in Use",true)==ROOM_VIS_IDLE);
 assert(room_visual_state("not_in_use",true)==ROOM_VIS_IDLE);
 assert(room_visual_state("Washing",true)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("Drying",true)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("Spinning",true)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("Rinsing",true)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("Paused",true)==ROOM_VIS_PAUSED);
 assert(room_visual_state("Finished",true)==ROOM_VIS_DONE);
 assert(room_visual_state("Error",true)==ROOM_VIS_ERROR);
 assert(room_visual_state("on",false)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("off",false)==ROOM_VIS_IDLE);
 assert(room_visual_state("opening",false)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("cleaning",false)==ROOM_VIS_ACTIVE);
 assert(room_visual_state("docked",false)==ROOM_VIS_IDLE);
 assert(room_visual_state("unavailable",true)==ROOM_VIS_UNKNOWN);
 assert(room_visual_state("Unrecognized status",true)==ROOM_VIS_UNKNOWN);
 puts("PASS: device and laundry state classes, including Washing and Not in Use");
 return 0;
}

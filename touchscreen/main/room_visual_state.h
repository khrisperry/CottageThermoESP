#pragma once
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
typedef enum {ROOM_VIS_IDLE,ROOM_VIS_ACTIVE,ROOM_VIS_PAUSED,ROOM_VIS_DONE,ROOM_VIS_ERROR,ROOM_VIS_UNKNOWN} room_visual_t;
static inline room_visual_t room_visual_state(const char *state,bool laundry)
{
    char normalized[128];unsigned at=0;
    for(const unsigned char *p=(const unsigned char*)state;*p&&at<sizeof(normalized)-1;++p)
        if(isalnum(*p))normalized[at++]=(char)tolower(*p);
    normalized[at]=0;
    if(!strcmp(normalized,"error")||!strcmp(normalized,"fault")||!strcmp(normalized,"problem")||!strcmp(normalized,"jammed"))return ROOM_VIS_ERROR;
    if(!strcmp(normalized,"paused")||!strcmp(normalized,"pause"))return ROOM_VIS_PAUSED;
    if(!strcmp(normalized,"finished")||!strcmp(normalized,"complete")||!strcmp(normalized,"completed")||!strcmp(normalized,"done")||!strcmp(normalized,"end"))return ROOM_VIS_DONE;
    if(!strcmp(normalized,"on")||!strcmp(normalized,"running")||!strcmp(normalized,"cleaning")||
       !strcmp(normalized,"opening")||!strcmp(normalized,"closing")||!strcmp(normalized,"returning")||
       (laundry&&(!strcmp(normalized,"washing")||!strcmp(normalized,"drying")||!strcmp(normalized,"rinsing")||
          !strcmp(normalized,"spinning")||!strcmp(normalized,"cooling")||!strcmp(normalized,"wash")||!strcmp(normalized,"dry")||
          !strcmp(normalized,"rinse")||!strcmp(normalized,"spin")||!strcmp(normalized,"inuse")||!strcmp(normalized,"cooldown"))))return ROOM_VIS_ACTIVE;
    if(!strcmp(normalized,"off")||!strcmp(normalized,"idle")||!strcmp(normalized,"notinuse")||
       !strcmp(normalized,"standby")||!strcmp(normalized,"ready")||!strcmp(normalized,"docked")||
       !strcmp(normalized,"open")||!strcmp(normalized,"closed")||!strcmp(normalized,"stopped"))return ROOM_VIS_IDLE;
    return ROOM_VIS_UNKNOWN;
}

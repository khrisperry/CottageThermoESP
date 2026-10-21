#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "room_visual_state.h"
typedef struct {bool active,want_on;int64_t since,until;} room_optimistic_t;
static inline void room_optimistic_begin(room_optimistic_t *o,bool on,int64_t now)
{*o=(room_optimistic_t){.active=true,.want_on=on,.since=now,.until=now+5000000};}
static inline bool room_optimistic_update(room_optimistic_t *o,const char *confirmed,int64_t sampled,int64_t now)
{
    if(o->active&&(now>=o->until||(sampled>o->since&&!strcmp(confirmed,o->want_on?"on":"off"))))o->active=false;
    return o->active;
}
typedef struct {bool armed,cycle_active,completed;int64_t seen;} room_laundry_tracker_t;
static inline bool room_laundry_observe(room_laundry_tracker_t *t,const char *state,bool available,int64_t now)
{
    t->completed=false;
    /* A gap cancels the sound, but must not erase a cycle awaiting auto-reset. */
    if(!available){t->armed=false;return false;}
    if(t->seen&&now-t->seen>30000000)t->armed=false;
    t->seen=now;
    room_visual_t v=room_visual_state(state,true);
    if(v==ROOM_VIS_ACTIVE){t->armed=true;t->cycle_active=true;return false;}
    /* Completed is also a real idle transition; repeated idle readings do not alert. */
    if(v==ROOM_VIS_IDLE||v==ROOM_VIS_DONE){
        bool notify=t->armed;t->completed=t->cycle_active;t->cycle_active=false;t->armed=false;return notify;
    }
    if(v==ROOM_VIS_ERROR||v==ROOM_VIS_UNKNOWN)t->armed=false;
    return false;
}

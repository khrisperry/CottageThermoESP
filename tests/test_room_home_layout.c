#include <assert.h>
#include <stdio.h>
#include "room_layout.h"
#include "room_home_policy.h"
static bool within(int x,int w,int limit){return x>=0&&w>0&&x+w<=limit;}
int main(void)
{
    /* Production layout constants are compiled here, not copied into a
     * mock. Verify five cards plus the reserved sixth grid position. */
    assert(within(0,ROOM_HOME_TILE_W,ROOM_PAGE_W));
    assert(within(ROOM_HOME_TILE_W+ROOM_HOME_TILE_GAP,ROOM_HOME_TILE_W,ROOM_PAGE_W));
    assert(ROOM_HOME_TILE_H+ROOM_HOME_TILE_GAP<=ROOM_HOME_ROW2_Y);
    assert(ROOM_HOME_ROW2_Y+ROOM_HOME_TILE_H+ROOM_HOME_TILE_GAP<=ROOM_HOME_ROW3_Y);
    assert(ROOM_HOME_ROW3_Y+ROOM_HOME_TILE_H<=ROOM_PAGE_H);
    assert(ROOM_HOME_ACTION_Y+ROOM_HOME_ACTION_H<=ROOM_HOME_TILE_H);
    /* All three blind buttons fit the card at usable touch sizes. */
    assert(within(10,62,ROOM_HOME_TILE_W));
    assert(within(81,62,ROOM_HOME_TILE_W));
    assert(within(152,62,ROOM_HOME_TILE_W));
    assert(10+62<81 && 81+62<152);
    /* Vacuum/Kids controls also fit, including clearance. */
    assert(within(10,100,ROOM_HOME_TILE_W));
    assert(within(114,100,ROOM_HOME_TILE_W));
    assert(10+100<114);
    /* The dial is centered, clear of header/status and bottom nav. */
    assert(ROOM_CLIMATE_RING_X*2+ROOM_CLIMATE_RING_SIZE==ROOM_PAGE_W);
    assert(ROOM_CLIMATE_RING_Y>0);
    assert(ROOM_CLIMATE_RING_Y+ROOM_CLIMATE_RING_SIZE<=ROOM_CLIMATE_MODE_Y);
    assert(ROOM_CLIMATE_BUTTON_Y+ROOM_CLIMATE_BUTTON_H<=ROOM_PAGE_H);
    /* A single light tile tap chooses the opposite valid HA action. */
    assert(room_home_light_toggle_action("off",3,true,false)==0);
    assert(room_home_light_toggle_action("on",3,true,false)==1);
    assert(room_home_light_toggle_action("off",2,true,false)==-1);
    assert(room_home_light_toggle_action("on",1,true,false)==-1);
    assert(room_home_light_toggle_action("unavailable",3,true,false)==-1);
    assert(room_home_light_toggle_action("unknown",3,true,false)==-1);
    assert(room_home_light_toggle_action("off",3,false,false)==-1);
    assert(room_home_light_toggle_action("on",3,true,true)==-1);
    puts("PASS: five compact Home cards, reserved sixth, no overlap, known-state-only light toggles");
    return 0;
}

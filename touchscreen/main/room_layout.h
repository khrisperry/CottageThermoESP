#pragma once
/* 480x480 Waveshare Touch LCD 4B. The page viewport begins at x=12,y=57
 * and is 456x364 pixels; navigation starts at y=429. Keep the control row
 * and LED tiles inside that viewport, and leave clearance for the LVGL
 * arc knob above the dial. Values are deliberately usable in host tests. */
enum {
    ROOM_PAGE_W=456, ROOM_PAGE_H=364,
    ROOM_CLIMATE_RING_X=102,
    ROOM_CLIMATE_RING_Y=13,
    ROOM_CLIMATE_RING_SIZE=252,
    ROOM_CLIMATE_MODE_Y=267,
    ROOM_CLIMATE_REASON_Y=292,
    ROOM_CLIMATE_STATUS_W=456,
    /* One bottom row: -1 | Set Temp | +1 | Away/Home.
     * Four full-height touch targets and equal 8px gutters. */
    ROOM_CLIMATE_BUTTON_Y=317,
    ROOM_CLIMATE_BUTTON_H=46,
    ROOM_CLIMATE_MINUS_X=0,
    ROOM_CLIMATE_MINUS_W=64,
    ROOM_CLIMATE_SET_X=72,
    ROOM_CLIMATE_SET_W=184,
    ROOM_CLIMATE_PLUS_X=264,
    ROOM_CLIMATE_PLUS_W=64,
    ROOM_CLIMATE_AWAY_X=336,
    ROOM_CLIMATE_AWAY_W=120,
    ROOM_HOME_TILE_W=224,
    ROOM_HOME_TILE_GAP=8,
    ROOM_HOME_TILE_H=108,
    ROOM_HOME_ROW2_Y=116,
    ROOM_HOME_ROW3_Y=236,
    ROOM_HOME_ACTION_Y=70,
    ROOM_HOME_ACTION_H=32,
    ROOM_HOME_ACTION_GAP=6
};
_Static_assert(ROOM_CLIMATE_RING_Y>=12,"Leave clearance for arc knob above dial");
_Static_assert(ROOM_CLIMATE_RING_X*2+ROOM_CLIMATE_RING_SIZE==ROOM_PAGE_W,"Center ring");
_Static_assert(ROOM_CLIMATE_RING_Y+ROOM_CLIMATE_RING_SIZE<=ROOM_CLIMATE_MODE_Y,
               "Dial must not overlap status text");
_Static_assert(ROOM_CLIMATE_STATUS_W<=ROOM_PAGE_W,
               "Mode and status must remain inside content width");
_Static_assert(ROOM_CLIMATE_AWAY_X+ROOM_CLIMATE_AWAY_W<=ROOM_PAGE_W,
               "Away/Home outside screen");
_Static_assert(ROOM_CLIMATE_PLUS_X+ROOM_CLIMATE_PLUS_W<ROOM_CLIMATE_AWAY_X,
               "Plus / Away overlap");
_Static_assert(ROOM_CLIMATE_BUTTON_Y>ROOM_CLIMATE_REASON_Y,
               "Bottom controls must not overlap status");
_Static_assert(ROOM_CLIMATE_MINUS_X+ROOM_CLIMATE_MINUS_W<ROOM_CLIMATE_SET_X,
               "Minus / Set overlap");
_Static_assert(ROOM_CLIMATE_SET_X+ROOM_CLIMATE_SET_W<ROOM_CLIMATE_PLUS_X,
               "Set / Plus overlap");
_Static_assert(ROOM_CLIMATE_AWAY_X+ROOM_CLIMATE_AWAY_W==ROOM_PAGE_W,
               "Four-button row should use the full content width");
_Static_assert(ROOM_CLIMATE_BUTTON_Y+ROOM_CLIMATE_BUTTON_H<=ROOM_PAGE_H,
               "Controls must clear bottom navigation");
_Static_assert(ROOM_HOME_TILE_W*2+ROOM_HOME_TILE_GAP==ROOM_PAGE_W,
               "Two equal columns must fill Home viewport");
_Static_assert(ROOM_HOME_TILE_H+ROOM_HOME_TILE_GAP<=ROOM_HOME_ROW2_Y,
               "Home first and second rows must not overlap");
_Static_assert(ROOM_HOME_ROW2_Y+ROOM_HOME_TILE_H+ROOM_HOME_TILE_GAP<=ROOM_HOME_ROW3_Y,
               "Home second and third rows must not overlap");
_Static_assert(ROOM_HOME_ROW3_Y+ROOM_HOME_TILE_H<=ROOM_PAGE_H,
               "Home grid must clear bottom navigation");
_Static_assert(ROOM_HOME_ACTION_Y+ROOM_HOME_ACTION_H<=ROOM_HOME_TILE_H,
               "Home actions must fit inside tiles");

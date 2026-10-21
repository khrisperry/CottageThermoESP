#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#define ROOM_SHIP_LIMIT 12
#define ROOM_SHIP_BODY_MAX 24576
typedef struct {
    uint32_t mmsi,age;char name[64],kind[32],destination[64];
    float lat,lon,distance,speed,course;bool speed_valid,course_valid,stale;
} room_ship_t;
typedef struct {
    bool service,feed;unsigned count,nearby,bytes,latency_ms,failures;
    int64_t received_at;float latitude,longitude,range;char error[80];
    room_ship_t ships[ROOM_SHIP_LIMIT];
} room_ships_t;
bool room_ships_decode(const char *body,size_t length,room_ships_t *out);
void room_ships_start(void);
void room_ships_active(bool active);
void room_ships_snapshot(room_ships_t *out);
bool room_ships_online(void);
/* Transfer a new cached map buffer to the LVGL owner; no network under its lock. */
void *room_ships_take_map(double bounds[4]);

#pragma once
#include <stdbool.h>
#include <string.h>

/* Only known, fresh On/Off HA states can be toggled. Unknown, stale,
 * unavailable or in-flight states must never guess what to command.
 * Action 0 = On and action 1 = Off (shared HA action map). */
static inline int room_home_light_toggle_action(const char *state,
                                                 unsigned actions,
                                                 bool available,
                                                 bool pending)
{
    if(!available || pending || !state)return -1;
    if(!strcmp(state,"off") && (actions&(1U<<0)))return 0;
    if(!strcmp(state,"on") && (actions&(1U<<1)))return 1;
    return -1;
}

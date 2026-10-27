#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define THERMO_MAP_MAX 8
#define THERMO_SCHEMA 2
#define THERMO_SCHEDULE_MAX 14
#define THERMO_TTL_MS 90000
/* Minimum spacing for intentional thermostat setpoint moves. Unlike adaptive
 * corrections, a new requested setpoint must not wait five minutes. */
#define THERMO_SETPOINT_GUARD_MS 2000U

typedef enum { TH_HOLD, TH_MANUAL, TH_DIRECT, TH_AUTO } th_mode_t;
typedef enum { TH_NONE, TH_ROOM, TH_SWITCHBOT } th_source_t;
typedef struct { float temp_f, level, pulse_us; } th_point_t;
typedef struct { uint8_t days; uint16_t minute; float target_f; } th_schedule_t;
typedef struct {
    uint32_t schema;
    th_mode_t mode;
    float target_f, direct_level;
    bool secondary_enabled;
    uint32_t dwell_ms;
    float level_deadband, max_level_step;
    size_t map_count;
    th_point_t map[THERMO_MAP_MAX];
    bool schedule_enabled;
    size_t schedule_count;
    th_schedule_t schedule[THERMO_SCHEDULE_MAX];
} th_config_t;
typedef struct { float f; uint64_t at_ms; bool valid; } th_sample_t;
typedef struct {
    th_config_t config;
    th_sample_t room, secondary;
    bool simulated, synchronized, has_command;
    float commanded_level;
    int commanded_us;
    uint64_t last_move_ms;
    th_source_t source;
    float selected_f, proposed_level;
    const char *reason;
    bool clock_valid;
    int weekday, minute; // Monday=0, local wall-clock minute of day
    float effective_target_f;
} th_state_t;
typedef struct { bool move; float level; int pulse_us; } th_action_t;

th_config_t th_defaults(void);
bool th_config_valid(const th_config_t *c);
void th_init(th_state_t *s, const th_config_t *c, bool simulated);
bool th_sample(th_state_t *s, th_source_t source, float f, uint32_t age_ms, uint64_t now);
bool th_sync(th_state_t *s, float known_level);
void th_release(th_state_t *s);
void th_clock(th_state_t *s, bool valid, int weekday, int minute);
bool th_scheduled_target(const th_config_t *c, int weekday, int minute, float *target);
th_action_t th_tick(th_state_t *s, uint64_t now);
void th_commit_move(th_state_t *s, th_action_t action, uint64_t now);
bool th_map_level(const th_config_t *c, float level, int *pulse);
const char *th_mode_name(th_mode_t mode);
const char *th_source_name(th_source_t source);

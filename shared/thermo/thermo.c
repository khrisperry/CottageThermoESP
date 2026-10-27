#include "thermo.h"
#include <math.h>
#include <string.h>

th_config_t th_defaults(void)
{
    return (th_config_t){ .schema=THERMO_SCHEMA, .mode=TH_HOLD, .target_f=65,
        .direct_level=3, .dwell_ms=300000, .level_deadband=0.1f, .max_level_step=0.25f };
}

bool th_config_valid(const th_config_t *c)
{
    if (c->schema != THERMO_SCHEMA || c->mode < TH_HOLD || c->mode > TH_AUTO ||
        !isfinite(c->target_f) || c->target_f < 45 || c->target_f > 80 ||
        !isfinite(c->direct_level) || c->direct_level < 1 || c->direct_level > 6 ||
        c->dwell_ms < 10000 || c->dwell_ms > 3600000 ||
        !isfinite(c->level_deadband) || c->level_deadband < 0.05f || c->level_deadband > 1 ||
        !isfinite(c->max_level_step) || c->max_level_step < 0.05f || c->max_level_step > 0.5f ||
        c->map_count > THERMO_MAP_MAX || c->map_count == 1) return false;
    int direction=0;
    for (size_t i=0; i<c->map_count; ++i) {
        th_point_t p=c->map[i];
        if (!isfinite(p.temp_f) || !isfinite(p.level) || !isfinite(p.pulse_us) ||
            p.temp_f < 40 || p.temp_f > 90 || p.level < 1 || p.level > 6 ||
            p.pulse_us < 500 || p.pulse_us > 2500) return false;
        if (i) {
            th_point_t prev=c->map[i-1];
            if (p.temp_f <= prev.temp_f || p.level <= prev.level ||
                fabsf(p.pulse_us-prev.pulse_us) < 1) return false;
            int d=p.pulse_us > prev.pulse_us ? 1 : -1;
            if (direction && direction != d) return false;
            direction=d;
        }
    }
    if (c->schedule_count > THERMO_SCHEDULE_MAX || (c->schedule_enabled && !c->schedule_count)) return false;
    for (size_t i=0;i<c->schedule_count;++i) {
        th_schedule_t p=c->schedule[i];
        if (!p.days || p.days>127 || p.minute>1439 || !isfinite(p.target_f) || p.target_f<45 || p.target_f>80) return false;
        for(size_t k=0;k<i;++k)
            if(c->schedule[k].minute==p.minute && (c->schedule[k].days&p.days)) return false;
    }
    return true;
}

void th_init(th_state_t *s, const th_config_t *c, bool simulated)
{
    memset(s,0,sizeof(*s));
    s->config=th_config_valid(c) ? *c : th_defaults();
    s->simulated=simulated;
    s->effective_target_f=s->config.target_f;
    s->reason="position_unsynchronized";
}

bool th_sample(th_state_t *s, th_source_t source, float f, uint32_t age_ms, uint64_t now)
{
    if ((source != TH_ROOM && source != TH_SWITCHBOT) || !isfinite(f) || f < 32 || f > 110 ||
        age_ms > THERMO_TTL_MS || age_ms > now) return false;
    th_sample_t *p=source == TH_ROOM ? &s->room : &s->secondary;
    // Delayed/reordered messages cannot replace a newer measurement.
    uint64_t measured=now-age_ms;
    if (p->valid && measured <= p->at_ms) return false;
    *p=(th_sample_t){.f=f,.at_ms=measured,.valid=true};
    return true;
}

bool th_map_level(const th_config_t *c, float level, int *pulse)
{
    if (!th_config_valid(c) || c->map_count < 2 || !isfinite(level) ||
        level < c->map[0].level || level > c->map[c->map_count-1].level) return false;
    for (size_t i=1;i<c->map_count;++i) {
        th_point_t a=c->map[i-1],b=c->map[i];
        if (level <= b.level) {
            *pulse=(int)lroundf(a.pulse_us+(b.pulse_us-a.pulse_us)*(level-a.level)/(b.level-a.level));
            return true;
        }
    }
    return false;
}

bool th_sync(th_state_t *s, float known_level)
{
    int pulse;
    if (!th_map_level(&s->config,known_level,&pulse)) return false;
    s->synchronized=true;
    s->has_command=true;
    s->commanded_level=known_level;
    s->commanded_us=pulse;
    s->last_move_ms=0;
    return true;
}

void th_release(th_state_t *s)
{
    s->synchronized=false;
    s->has_command=false;
}

void th_clock(th_state_t *s,bool valid,int weekday,int minute)
{
    s->clock_valid=valid && weekday>=0 && weekday<7 && minute>=0 && minute<1440;
    s->weekday=weekday; s->minute=minute;
}

bool th_scheduled_target(const th_config_t *c,int weekday,int minute,float *target)
{
    if(!th_config_valid(c) || !c->schedule_count || weekday<0 || weekday>6 || minute<0 || minute>1439)return false;
    int best=10081;bool found=false;
    for(size_t i=0;i<c->schedule_count;++i)for(int d=0;d<7;++d) {
        if(!(c->schedule[i].days&(1<<d)))continue;
        int age=(weekday*1440+minute-(d*1440+c->schedule[i].minute)+10080)%10080;
        if(age<best){best=age;*target=c->schedule[i].target_f;found=true;}
    }
    return found;
}

static bool fresh(th_sample_t p,uint64_t now)
{
    return p.valid && now >= p.at_ms && now-p.at_ms <= THERMO_TTL_MS;
}

th_action_t th_tick(th_state_t *s,uint64_t now)
{
    th_action_t a={0};
    s->effective_target_f=s->config.target_f;
    s->source=TH_NONE;
    s->selected_f=NAN;
    s->proposed_level=NAN;
    bool room=fresh(s->room,now),secondary=s->config.secondary_enabled && fresh(s->secondary,now);
    if (room) { s->source=TH_ROOM; s->selected_f=s->room.f; }
    else if (secondary) { s->source=TH_SWITCHBOT; s->selected_f=s->secondary.f; }
    if (s->config.mode == TH_MANUAL || s->config.mode == TH_HOLD) {
        s->reason=s->config.mode == TH_MANUAL ? "manual" : "hold";
        return a;
    }
    if (s->config.map_count < 2) { s->reason="calibration_required"; return a; }
    if (!s->synchronized) { s->reason="position_unsynchronized"; return a; }
    float desired=s->config.direct_level;
    if (s->config.mode == TH_AUTO) {
        if (s->source == TH_NONE) { s->reason="temperature_stale"; return a; }
        if (room && secondary && fabsf(s->room.f-s->secondary.f) > 5) {
            s->reason="sensor_disagreement"; return a;
        }
        float target=s->config.target_f;
        if(s->config.schedule_enabled) {
            if(!s->clock_valid || !th_scheduled_target(&s->config,s->weekday,s->minute,&target)) {
                s->reason="schedule_clock_unavailable";return a;
            }
        }
        s->effective_target_f=target;
        if (target < s->config.map[0].temp_f || target > s->config.map[s->config.map_count-1].temp_f) {
            s->reason="target_outside_measured_map"; return a;
        }
        // Set Williams' own thermostat to empirical steady-state level.
        // Do not treat the servo as a room-temperature on/off relay or integrate error.
        for (size_t i=1;i<s->config.map_count;++i) {
            th_point_t x=s->config.map[i-1],y=s->config.map[i];
            if (target <= y.temp_f) {
                desired=x.level+(y.level-x.level)*(target-x.temp_f)/(y.temp_f-x.temp_f);
                break;
            }
        }
    }
    int pulse;
    if (!th_map_level(&s->config,desired,&pulse)) { s->reason="level_outside_measured_map"; return a; }
    s->proposed_level=desired;
    if (fabsf(desired-s->commanded_level) < s->config.level_deadband) { s->reason="mapped_setting_reached"; return a; }
    if (s->last_move_ms && now-s->last_move_ms < s->config.dwell_ms) { s->reason="movement_dwell"; return a; }
    float delta=desired-s->commanded_level;
    if (fabsf(delta)>s->config.max_level_step) desired=s->commanded_level+copysignf(s->config.max_level_step,delta);
    if (!th_map_level(&s->config,desired,&pulse)) { s->reason="invalid_move"; return a; }
    s->reason=s->simulated ? "simulated_move" : "bench_move";
    return (th_action_t){.move=true,.level=desired,.pulse_us=pulse};
}

void th_commit_move(th_state_t *s,th_action_t a,uint64_t now)
{
    if (!a.move) return;
    s->has_command=true;
    s->commanded_level=a.level;
    s->commanded_us=a.pulse_us;
    s->last_move_ms=now;
}

const char *th_mode_name(th_mode_t m)
{
    const char *names[]={"hold","manual","direct","thermostat"};
    return m>=TH_HOLD && m<=TH_AUTO ? names[m] : "invalid";
}
const char *th_source_name(th_source_t s)
{
    return s==TH_ROOM ? "room" : s==TH_SWITCHBOT ? "switchbot" : "none";
}

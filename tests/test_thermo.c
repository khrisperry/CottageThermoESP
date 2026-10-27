#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "thermo.h"

static th_config_t example(void)
{
    th_config_t c=th_defaults();c.mode=TH_AUTO;c.map_count=3;
    // Synthetic test fixture ONLY. These are not furnace calibration measurements.
    c.map[0]=(th_point_t){50,1,1200};c.map[1]=(th_point_t){65,3,1500};c.map[2]=(th_point_t){80,6,1800};
    c.target_f=70;return c;
}
static th_state_t prepared(void)
{
    th_config_t c=example();th_state_t s;th_init(&s,&c,true);
    assert(th_sample(&s,TH_ROOM,60,0,100000));assert(th_sync(&s,3));return s;
}
static void expect_reason(th_state_t *s,uint64_t now,const char *reason)
{
    assert(!th_tick(s,now).move);assert(!strcmp(s->reason,reason));
}
int main(void)
{
    th_config_t c=example();assert(th_config_valid(&c));
    th_state_t s;th_init(&s,&c,true);expect_reason(&s,100000,"position_unsynchronized");
    assert(!s.synchronized && !s.has_command); // Never restore a persisted target as position.
    /* A thermostat setpoint maps straight to the calibrated dial position,
     * without 0.25-level steps or the five-minute adaptive dwell. */
    s=prepared();th_action_t a=th_tick(&s,100000);
    assert(a.move&&fabsf(a.level-4.0f)<0.001f&&a.pulse_us==1600);
    th_commit_move(&s,a,100000);assert(s.commanded_us==1600);
    expect_reason(&s,101000,"mapped_setting_reached");
    s.config.target_f=80;
    expect_reason(&s,101000,"setpoint_cooldown");
    assert(th_tick(&s,101999).move==false);
    a=th_tick(&s,102000);assert(a.move&&a.level==6&&a.pulse_us==1800);
    th_commit_move(&s,a,102000);
    s.config.target_f=50;expect_reason(&s,103000,"setpoint_cooldown");
    a=th_tick(&s,104000);assert(a.move&&a.level==1&&a.pulse_us==1200);
    th_commit_move(&s,a,104000);
    assert(th_sample(&s,TH_ROOM,61,0,105000));
    expect_reason(&s,105000,"mapped_setting_reached");
    s.config.target_f=65;
    expect_reason(&s,105000,"setpoint_cooldown");
    a=th_tick(&s,106000);assert(a.move&&a.level==3&&a.pulse_us==1500);
    /* The room can warm/cool without inventing any additional dial moves. */
    th_commit_move(&s,a,106000);
    assert(th_sample(&s,TH_ROOM,56,0,108000));expect_reason(&s,108000,"mapped_setting_reached");
    s=prepared();expect_reason(&s,190001,"temperature_stale");
    assert(th_sample(&s,TH_SWITCHBOT,62,0,190001));expect_reason(&s,190001,"temperature_stale");
    s.config.secondary_enabled=true;assert(th_tick(&s,190001).move&&s.source==TH_SWITCHBOT);
    assert(th_sample(&s,TH_ROOM,70,0,190002));expect_reason(&s,190002,"sensor_disagreement");
    s.config.secondary_enabled=false;assert(th_tick(&s,190002).move&&s.source==TH_ROOM);
    s.config.target_f=45;expect_reason(&s,190002,"target_outside_measured_map");
    /* Non-setpoint/direct and future background adjustments keep the original
     * dwell and fractional movement safety policy. */
    s=prepared();s.config.mode=TH_DIRECT;s.config.direct_level=6;assert(th_tick(&s,300000).move);
    s.config.direct_level=1;assert(th_tick(&s,300000).level==2.75f);
    s.config.direct_level=6;a=th_tick(&s,300000);assert(a.move&&a.level==3.25f);
    th_commit_move(&s,a,300000);
    s.config.direct_level=1;expect_reason(&s,301000,"movement_dwell");
    s.config.direct_level=0;expect_reason(&s,300000,"level_outside_measured_map");
    s=prepared();s.config.mode=TH_HOLD;expect_reason(&s,100000,"hold");
    s.config.mode=TH_MANUAL;expect_reason(&s,100000,"manual");
    s=prepared();th_release(&s);expect_reason(&s,100000,"position_unsynchronized");
    s=prepared();s.config.target_f=65;expect_reason(&s,100000,"mapped_setting_reached");
    assert(!th_sample(&s,TH_NONE,60,0,100001));assert(!th_sample(&s,TH_ROOM,NAN,0,100001));
    assert(!th_sample(&s,TH_ROOM,20,0,100001));assert(!th_sample(&s,TH_ROOM,111,0,100001));
    assert(!th_sample(&s,TH_ROOM,60,90001,200000));assert(!th_sample(&s,TH_ROOM,60,5,4));
    assert(!th_sample(&s,TH_ROOM,60,0,99999));assert(!th_sample(&s,TH_ROOM,60,0,100000));
    assert(th_sample(&s,TH_ROOM,60,1,100002));
    assert(!th_sync(&s,0));assert(!th_sync(&s,NAN));assert(!th_sync(&s,6.1));
    int pulse;assert(th_map_level(&c,1,&pulse)&&pulse==1200);assert(th_map_level(&c,6,&pulse)&&pulse==1800);
    assert(th_map_level(&c,2,&pulse)&&pulse==1350);assert(!th_map_level(&c,NAN,&pulse));
    c.map[0].pulse_us=1800;c.map[2].pulse_us=1200;assert(th_config_valid(&c));
    assert(th_map_level(&c,2,&pulse)&&pulse==1650); // Reverse servo direction supported.
    c.map[1].pulse_us=1850;assert(!th_config_valid(&c));
    c=example();c.map[1].level=1;assert(!th_config_valid(&c));
    c=example();c.map[1].temp_f=50;assert(!th_config_valid(&c));
    c=example();c.map[0].pulse_us=499;assert(!th_config_valid(&c));
    c=example();c.map[0].pulse_us=500;assert(th_config_valid(&c));
    c=example();c.map[2].pulse_us=2500;assert(th_config_valid(&c));
    c.map[2].pulse_us=2501;assert(!th_config_valid(&c));
    c=example();c.target_f=NAN;assert(!th_config_valid(&c));
    c=example();c.max_level_step=INFINITY;assert(!th_config_valid(&c));
    c=example();c.map_count=1;assert(!th_config_valid(&c));c.map_count=9;assert(!th_config_valid(&c));
    c=example();c.schema=99;assert(!th_config_valid(&c));
    th_init(&s,&c,true);assert(s.config.mode==TH_HOLD&&s.config.map_count==0);
    c=example();c.map_count=0;th_init(&s,&c,true);expect_reason(&s,100000,"calibration_required");
    c=example();c.schedule_enabled=true;c.schedule_count=2;
    c.schedule[0]=(th_schedule_t){.days=127,.minute=360,.target_f=68};
    c.schedule[1]=(th_schedule_t){.days=127,.minute=1320,.target_f=55};
    assert(th_config_valid(&c));float target=0;
    assert(th_scheduled_target(&c,0,100,&target)&&target==55); // Sunday wraps to Monday.
    assert(th_scheduled_target(&c,2,360,&target)&&target==68);
    assert(th_scheduled_target(&c,6,1320,&target)&&target==55);
    assert(!th_scheduled_target(&c,7,360,&target));
    th_init(&s,&c,true);assert(th_sync(&s,3));assert(th_sample(&s,TH_ROOM,60,0,100000));
    expect_reason(&s,100000,"schedule_clock_unavailable");
    th_clock(&s,true,0,360);
    a=th_tick(&s,100000);
    assert(a.move && fabsf(a.level-3.6f)<0.001f);
    assert(s.effective_target_f==68);
    th_commit_move(&s,a,100000);
    /* A scheduled setback is also an intentional setpoint change, not a
     * five-minute series of 0.25-level moves. */
    th_clock(&s,true,0,1320);
    expect_reason(&s,101000,"setpoint_cooldown");
    a=th_tick(&s,102000);
    assert(a.move && fabsf(a.level-(1.0f+5.0f/15.0f*2.0f))<0.001f);
    th_clock(&s,true,9,360);expect_reason(&s,102000,"schedule_clock_unavailable");
    c.schedule[1].minute=360;assert(!th_config_valid(&c)); // Overlap rejected.
    c.schedule[1].days=0;assert(!th_config_valid(&c));
    // Long simulation of conservative DIRECT mode: bounded steps and dwell.
    s=prepared();s.config.mode=TH_DIRECT;s.config.direct_level=4;
    float previous=s.commanded_level;uint64_t last=0;
    for(uint64_t now=100000;now<4000000;now+=1000) {
        assert(th_sample(&s,TH_ROOM,60,0,now+1));
        a=th_tick(&s,now+1);
        if(a.move) {
            assert(fabsf(a.level-previous)<=0.2501f);assert(a.pulse_us>=1200&&a.pulse_us<=1800);
            if(last)assert(now+1-last>=s.config.dwell_ms);
            th_commit_move(&s,a,now+1);previous=a.level;last=now+1;
        }
    }
    assert(fabsf(s.commanded_level-4)<0.001f);
    puts("PASS: setpoint full-travel response with 2-second guard, slow direct-mode dwell, maps, freshness, safety checks, and long simulation");
}

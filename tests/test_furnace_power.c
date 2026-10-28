#include <assert.h>
#include <stdio.h>
#include "room_furnace_power.h"
static const int64_t base=1767225600000LL;
static void sample(fp_tracker_t *s,int sec,double watts)
{fp_update(s,watts,base+sec*1000,base+sec*1000,180);}
int main(void)
{
    assert(fp_report_ms("2026-01-01T00:00:00Z")==base);
    assert(fp_report_ms("2025-12-31T19:00:00-05:00")==base);
    assert(fp_report_ms("2026-01-01T01:00:00+01:00")==base);
    assert(fp_report_ms("2026-01-01T00:00:00.123456+00:00")==base+123);
    assert(fp_report_ms("2026-01-01T00:00:00.1Z")==base+100);
    const char *invalid[]={"","2026-02-29T00:00:00Z","2026-00-01T00:00:00Z",
        "2026-01-01T24:00:00Z","2026-01-01T00:00:00+24:00",
        "2026-01-01T00:00:00.Z","2026-01-01T00:00:00","2026-01-01T00:00:00Zjunk"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)assert(!fp_report_ms(invalid[i]));
    assert(fp_report_ms("2028-02-29T00:00:00Z")>base);
    fp_tracker_t s={0};
    sample(&s,0,0.1);assert(s.stage==FP_UNKNOWN);
    fp_update(&s,0.1,base,base+11000,180);assert(s.stage==FP_UNKNOWN); /* repeated cache */
    sample(&s,11,0.1);assert(s.stage==FP_STANDBY);
    sample(&s,12,3);assert(s.stage==FP_STANDBY); /* hysteresis */
    sample(&s,13,18);sample(&s,24,37);assert(s.stage==FP_STARTUP);
    sample(&s,25,134);sample(&s,36,128);assert(s.stage==FP_HEATING);
    sample(&s,37,112);sample(&s,48,111);assert(s.stage==FP_HEATING);
    sample(&s,49,93);sample(&s,60,91);assert(s.stage==FP_COOLDOWN);
    sample(&s,61,0.1);sample(&s,72,0.2);assert(s.stage==FP_STANDBY&&s.completed_cycles==1);
    sample(&s,90,0.1);assert(s.completed_cycles==1);
    /* A 90W boot sample cannot assert a completed heating cycle. */
    s=(fp_tracker_t){0};sample(&s,0,90);sample(&s,11,91);
    assert(s.stage==FP_UNCLASSIFIED&&s.completed_cycles==0);
    /* Brief spikes cannot assert likely heating. */
    sample(&s,12,130);sample(&s,14,90);sample(&s,25,91);
    assert(s.stage==FP_UNCLASSIFIED&&!s.heating_seen);
    /* A data gap severs cycle context, preserving only completed counts. */
    sample(&s,26,130);sample(&s,37,129);assert(s.heating_seen);
    fp_update(&s,129,base+37000,base+218000,180);assert(s.stage==FP_UNKNOWN&&!s.heating_seen);
    sample(&s,219,90);sample(&s,230,91);assert(s.stage==FP_UNCLASSIFIED);
    sample(&s,231,0.1);sample(&s,242,0.1);assert(s.completed_cycles==0);
    /* Invalid/future/no-clock evidence is unknown, never standby. */
    sample(&s,243,NAN);assert(s.stage==FP_UNKNOWN);
    sample(&s,244,-1);assert(s.stage==FP_UNKNOWN);
    fp_update(&s,130,base+260000,base+244000,180);assert(s.stage==FP_UNKNOWN);
    fp_update(&s,130,base,0,180);assert(s.stage==FP_UNKNOWN);
    sample(&s,270,500);sample(&s,281,500);assert(s.stage==FP_UNCLASSIFIED);
    sample(&s,282,130);sample(&s,293,129);assert(s.stage==FP_HEATING);
    fp_update(&s,0.1,base+280000,base+294000,180);assert(s.stage==FP_HEATING); /* out of order */
    puts("PASS: furnace history bands, timestamp/offset parsing, source freshness, cache rejection, debounce, hysteresis, inferred cooldown, complete cycles and gap/anomaly handling");
}

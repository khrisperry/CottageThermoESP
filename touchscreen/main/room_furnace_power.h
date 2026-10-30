#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

/* Provisional electrical bands derived from the supplied 2026-10-27 history.
 * These identify equipment stages, not verified gas/flame state. */
typedef enum {FP_UNKNOWN,FP_STANDBY,FP_STARTUP,FP_HEATING,FP_COOLDOWN,
              FP_TRANSITION,FP_UNCLASSIFIED} fp_stage_t;
typedef struct {
    fp_stage_t stage,candidate;
    int64_t last_report_ms,candidate_since_ms,stage_since_ms;
    bool heating_seen,cooldown_seen;
    unsigned completed_cycles;
} fp_tracker_t;
static inline const char *fp_stage_name(fp_stage_t s)
{
    const char *names[]={"unknown","standby","startup","likely_heating",
        "likely_cooldown","transition","unclassified_load"};
    return s>=FP_UNKNOWN&&s<=FP_UNCLASSIFIED?names[s]:"unknown";
}
static inline void fp_invalidate(fp_tracker_t *s)
{
    unsigned cycles=s->completed_cycles;
    memset(s,0,sizeof(*s));s->completed_cycles=cycles;
}
static inline bool fp_source_fresh(int64_t reported,int64_t now,unsigned max_age_s)
{
    return reported>0&&now>=1767225600000LL&&reported<=now+5000&&
        now-reported<=(int64_t)max_age_s*1000;
}
static inline fp_stage_t fp_band(const fp_tracker_t *s,double watts)
{
    if(watts<=2 || (s->stage==FP_STANDBY&&watts<5))return FP_STANDBY;
    if(watts>=5&&watts<=50)return FP_STARTUP;
    if(watts<=160&&(watts>=115 || (s->stage==FP_HEATING&&watts>=110)))return FP_HEATING;
    if(watts>=75&&(watts<=105 || (s->stage==FP_COOLDOWN&&watts<=110)))
        return s->heating_seen?FP_COOLDOWN:FP_UNCLASSIFIED;
    return watts>160?FP_UNCLASSIFIED:FP_TRANSITION;
}
static inline void fp_update(fp_tracker_t *s,double watts,int64_t reported,int64_t now,unsigned max_age_s)
{
    if(!isfinite(watts)||watts<0||!fp_source_fresh(reported,now,max_age_s)){
        fp_invalidate(s);return;
    }
    /* A cached HA response cannot confirm a stage. Out-of-order samples
     * cannot rewind the tracker or complete a cycle. */
    if(reported<=s->last_report_ms)return;
    if(s->last_report_ms&&reported-s->last_report_ms>(int64_t)max_age_s*1000)
        fp_invalidate(s);
    s->last_report_ms=reported;
    fp_stage_t next=fp_band(s,watts);
    if(next!=s->candidate || !s->candidate_since_ms){
        s->candidate=next;s->candidate_since_ms=reported;return;
    }
    /* Two distinct source reports spanning >=10 seconds reject brief spikes. */
    if(reported-s->candidate_since_ms<10000 || next==s->stage)return;
    s->stage=next;s->stage_since_ms=reported;
    if(next==FP_HEATING)s->heating_seen=true;
    if(next==FP_COOLDOWN)s->cooldown_seen=true;
    if(next==FP_STANDBY){
        if(s->heating_seen&&s->cooldown_seen)++s->completed_cycles;
        s->heating_seen=false;s->cooldown_seen=false;
    }
}
static inline int fp_digits(const char *p,int n)
{
    int v=0;
    for(int i=0;i<n;++i){if(p[i]<'0'||p[i]>'9')return -1;v=v*10+p[i]-'0';}
    return v;
}
/* Strict RFC3339 report timestamp, independent of device/local timezone. */
static inline int64_t fp_report_ms(const char *p)
{
    if(!p||strlen(p)<20||p[4]!='-'||p[7]!='-'||p[10]!='T'||p[13]!=':'||p[16]!=':')return 0;
    int y=fp_digits(p,4),m=fp_digits(p+5,2),d=fp_digits(p+8,2);
    int h=fp_digits(p+11,2),min=fp_digits(p+14,2),sec=fp_digits(p+17,2);
    bool leap=y%4==0&&(y%100!=0||y%400==0);
    const int mdays[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<2020||y>2200||m<1||m>12||d<1||d>mdays[m-1]+(m==2&&leap)||
       h<0||h>23||min<0||min>59||sec<0||sec>59)return 0;
    const char *q=p+19;int ms=0;
    if(*q=='.'){
        ++q;int n=0;
        while(*q>='0'&&*q<='9'){if(n<3)ms=ms*10+*q-'0';++q;++n;}
        if(!n)return 0;
        for(;n<3;++n)ms*=10;
    }
    int offset=0;
    if(*q=='Z'){if(q[1])return 0;}
    else if(*q=='+'||*q=='-'){
        if(strlen(q)!=6||q[3]!=':')return 0;
        int oh=fp_digits(q+1,2),om=fp_digits(q+4,2);
        if(oh<0||oh>23||om<0||om>59)return 0;
        offset=(oh*60+om)*(*q=='+'?1:-1);
    }else return 0;
    int64_t days=0;
    for(int year=1970;year<y;++year)days+=365+(year%4==0&&(year%100!=0||year%400==0));
    for(int month=1;month<m;++month)days+=mdays[month-1]+(month==2&&leap);
    days+=d-1;
    return ((days*24+h)*3600+min*60+sec-offset*60)*1000+ms;
}

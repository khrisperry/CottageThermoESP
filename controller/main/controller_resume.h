/* Persisted dial-command checkpoint for encoderless boot recovery.
 * This is an ASSUMED servo position, never a measured knob position.
 * A reset during a movement leaves the checkpoint in PENDING state and
 * permanently blocks remote rearm until physical resynchronization. */
#pragma once
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "controller_setup.h"

#define CONTROLLER_RESUME_MAGIC 0x43525331U
#define CONTROLLER_RESUME_SETTLE_MS 4000U
#define CONTROLLER_RESUME_PENDING 1U
#define CONTROLLER_RESUME_STABLE 2U

typedef struct {
    uint32_t magic;
    uint32_t setup_hash;
    float last_level;
    int32_t last_pulse_us;
    uint32_t phase;
} controller_resume_checkpoint_t;

/* Hash only the calibration bytes, not software target or mode. In particular,
 * a map change must invalidate any older position assumption. */
static inline uint32_t controller_resume_setup_hash(const controller_setup_t *setup)
{
    const unsigned char *p=(const unsigned char *)setup;
    uint32_t h=2166136261U;
    for(size_t i=0;i<sizeof(*setup);++i)h=(h^p[i])*16777619U;
    return h;
}
static inline controller_resume_checkpoint_t controller_resume_make(
    const controller_setup_t *setup,float level,int pulse_us,bool stable)
{
    return (controller_resume_checkpoint_t){
        .magic=CONTROLLER_RESUME_MAGIC,
        .setup_hash=controller_resume_setup_hash(setup),
        .last_level=level,
        .last_pulse_us=pulse_us,
        .phase=stable?CONTROLLER_RESUME_STABLE:CONTROLLER_RESUME_PENDING
    };
}
static inline bool controller_resume_valid(
    const controller_resume_checkpoint_t *c,const controller_setup_t *setup)
{
    if(!c || !setup || c->magic!=CONTROLLER_RESUME_MAGIC ||
       c->phase!=CONTROLLER_RESUME_STABLE ||
       c->setup_hash!=controller_resume_setup_hash(setup) ||
       !isfinite(c->last_level) ||
       c->last_level<1.0f || c->last_level>6.0f ||
       !controller_setup_contains(setup,c->last_pulse_us) ||
       controller_setup_ready(setup)!=NULL)return false;
    th_config_t cfg=th_defaults();
    int mapped=0;
    return controller_setup_map(setup,&cfg) &&
           th_map_level(&cfg,c->last_level,&mapped) &&
           (mapped-c->last_pulse_us<=1 && mapped-c->last_pulse_us>=-1);
}

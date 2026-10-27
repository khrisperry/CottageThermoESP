#pragma once
#include "thermo.h"

#define CONTROLLER_SETUP_MAGIC 0x43535431U
/* Off, 1..6, Max. Endpoints and references never command motion on load. */
typedef struct {
    uint32_t magic;
    int start_us, end_us;
    int pulse_us[8];
    float angle_deg[8];
    uint32_t angle_mask;
    float temperature_f[6];
    uint32_t temperature_mask;
} controller_setup_t;
controller_setup_t controller_setup_defaults(void);
bool controller_setup_valid(const controller_setup_t *s);
bool controller_setup_contains(const controller_setup_t *s, int us);
const char *controller_setup_ready(const controller_setup_t *s);
bool controller_setup_map(const controller_setup_t *s, th_config_t *config);

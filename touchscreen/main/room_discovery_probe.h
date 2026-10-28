#pragma once
#include <stdbool.h>
#include "esp_err.h"

/* Identity verification of an mDNS-discovered, untrusted HTTP origin.
 * This does not send commands, change NVS or bypass paired ESP-NOW. */
typedef struct {
    esp_err_t http_error;
    int http_status;
    int response_bytes;
    bool identity_mismatch;
} room_discovery_result_t;

/* /api/status is a full diagnostics response and now exceeds the former
 * 1800-byte stack buffer. Use a bounded heap buffer (8 KB, not task stack)
 * so a 200 OK with a complete, authenticated identity can be recognized.
 * Expected MAC is enforced whenever a radio peer is configured. */
bool room_discovery_probe_origin(const char *origin, bool check_peer_mac,
                                 const char *expected_mac,
                                 room_discovery_result_t *result);

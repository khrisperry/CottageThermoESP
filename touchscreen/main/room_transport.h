#pragma once
#include "esp_err.h"
#include <stddef.h>
/* Caller owns response storage and HTTP client; never retries a mutation. */
esp_err_t room_http(const char *origin, const char *header, const char *token,
                    const char *path, const char *post, void *out, size_t capacity,
                    int timeout_ms, int *status, int *length);

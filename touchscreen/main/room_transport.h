#pragma once
#include "esp_err.h"
#include <stddef.h>
/* Caller owns response storage; requests are never automatically replayed. */
esp_err_t room_http(const char *origin, const char *header, const char *token,
                    const char *path, const char *post, void *out, size_t capacity,
                    int timeout_ms, int *status, int *length);

/* One origin per session. Caller must serialize all access to a session.
 * HTTP and HTTPS supported; NULL header/token omits authentication. */
typedef struct room_http_session room_http_session_t;
esp_err_t room_http_keepalive(room_http_session_t **session,
                    const char *origin, const char *header, const char *token,
                    const char *path, const char *post, void *out, size_t capacity,
                    int timeout_ms, int *status, int *length);

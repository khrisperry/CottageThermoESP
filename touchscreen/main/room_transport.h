#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>
#include "room_http_observe_policy.h"
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

/* Optional phase information for the independent read-only learning GET.
 * Reusing an HTTP client handle does not prove TCP socket reuse. */
typedef struct {
    room_http_stage_t stage;
    bool client_handle_reused;
    bool request_headers_sent;
    bool response_headers_complete;
    bool response_body_started;
    size_t response_bytes;
} room_http_diagnostic_t;

esp_err_t room_http_keepalive_diagnostic(room_http_session_t **session,
                    const char *origin, const char *header, const char *token,
                    const char *path, const char *post, void *out, size_t capacity,
                    int timeout_ms, int *status, int *length,
                    room_http_diagnostic_t *diagnostic);

#pragma once
#include <stdbool.h>

/* Stage reached before a passive HTTP read failed. This classifies the
 * observable phase, not the precise TCP/Wi-Fi root cause. */
typedef enum {
    ROOM_HTTP_STAGE_OK=0,
    ROOM_HTTP_STAGE_CONNECT,
    ROOM_HTTP_STAGE_HEADERS,
    ROOM_HTTP_STAGE_BODY,
    ROOM_HTTP_STAGE_OVERFLOW
} room_http_stage_t;

static inline room_http_stage_t room_http_observe_stage(
    bool request_failed,bool request_headers_sent,
    bool response_headers_complete,bool body_overflow,bool complete)
{
    if(body_overflow)return ROOM_HTTP_STAGE_OVERFLOW;
    if(request_failed){
        if(response_headers_complete)return ROOM_HTTP_STAGE_BODY;
        if(request_headers_sent)return ROOM_HTTP_STAGE_HEADERS;
        return ROOM_HTTP_STAGE_CONNECT;
    }
    if(!complete)return ROOM_HTTP_STAGE_BODY;
    return ROOM_HTTP_STAGE_OK;
}
static inline const char *room_http_observe_reason(room_http_stage_t stage)
{
    switch(stage){
    case ROOM_HTTP_STAGE_OK:return "ok";
    case ROOM_HTTP_STAGE_CONNECT:return "http_connect_failure";
    case ROOM_HTTP_STAGE_HEADERS:return "http_headers_failure";
    case ROOM_HTTP_STAGE_BODY:return "http_body_failure";
    case ROOM_HTTP_STAGE_OVERFLOW:return "http_body_overflow";
    default:return "http_unknown_failure";
    }
}

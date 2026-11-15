#include <assert.h>
#include <string.h>
#include "room_http_observe_policy.h"

int main(void)
{
    assert(room_http_observe_stage(false,true,true,false,true)==ROOM_HTTP_STAGE_OK);
    /* A request that never sent headers is classified as a connect/send issue. */
    assert(room_http_observe_stage(true,false,false,false,false)==ROOM_HTTP_STAGE_CONNECT);
    /* TCP established and request sent, but no complete response headers. */
    assert(room_http_observe_stage(true,true,false,false,false)==ROOM_HTTP_STAGE_HEADERS);
    /* Response headers arrived, body stalled or was incomplete. */
    assert(room_http_observe_stage(true,true,true,false,false)==ROOM_HTTP_STAGE_BODY);
    assert(room_http_observe_stage(false,true,true,false,false)==ROOM_HTTP_STAGE_BODY);
    assert(room_http_observe_stage(true,true,true,true,false)==ROOM_HTTP_STAGE_OVERFLOW);
    assert(room_http_observe_stage(false,true,true,true,true)==ROOM_HTTP_STAGE_OVERFLOW);
    assert(!strcmp(room_http_observe_reason(ROOM_HTTP_STAGE_SETUP),"http_setup_failure"));
    assert(!strcmp(room_http_observe_reason(ROOM_HTTP_STAGE_CONNECT),"http_connect_failure"));
    assert(!strcmp(room_http_observe_reason(ROOM_HTTP_STAGE_HEADERS),"http_headers_failure"));
    assert(!strcmp(room_http_observe_reason(ROOM_HTTP_STAGE_BODY),"http_body_failure"));
    assert(!strcmp(room_http_observe_reason(ROOM_HTTP_STAGE_OVERFLOW),"http_body_overflow"));
    return 0;
}

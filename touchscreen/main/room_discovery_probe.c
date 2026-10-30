#include "room_discovery_probe.h"
#include "room_transport.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ROOM_DISCOVERY_STATUS_CAPACITY 8192

bool room_discovery_probe_origin(const char *origin, bool check_peer_mac,
                                 const char *expected_mac,
                                 room_discovery_result_t *result)
{
    if (!result) return false;
    *result=(room_discovery_result_t){.http_error=ESP_ERR_INVALID_ARG};
    if (!origin || !*origin) return false;
    /* This may run inside a 6144-byte controller task. Never put the
     * controller's expanding /api/status diagnostic response on its stack.
     * Keep a hard limit to avoid an unbounded or malicious LAN response. */
    char *body=malloc(ROOM_DISCOVERY_STATUS_CAPACITY);
    if (!body) {
        result->http_error=ESP_ERR_NO_MEM;
        return false;
    }
    int status=0,length=0;
    esp_err_t err=room_http(origin,NULL,NULL,"/api/status",NULL,
                            body,ROOM_DISCOVERY_STATUS_CAPACITY,2400,
                            &status,&length);
    result->http_error=err;
    result->http_status=status;
    result->response_bytes=length;
    if (err!=ESP_OK || status!=200 || length<=0 ||
        length>=(int)ROOM_DISCOVERY_STATUS_CAPACITY) {
        free(body);
        return false;
    }
    body[length]=0;
    cJSON *root=cJSON_ParseWithLengthOpts(body,length+1,NULL,true);
    free(body);
    const cJSON *net=cJSON_GetObjectItemCaseSensitive(root,"network");
    const cJSON *hostname=cJSON_GetObjectItemCaseSensitive(net,"hostname");
    const cJSON *mac=cJSON_GetObjectItemCaseSensitive(net,"station_mac");
    bool name_ok=cJSON_IsString(hostname) &&
        !strcmp(hostname->valuestring,"CottageThermoController");
    bool pair_ok=!check_peer_mac || !expected_mac || !*expected_mac ||
        (cJSON_IsString(mac)&&!strcasecmp(mac->valuestring,expected_mac));
    cJSON_Delete(root);
    result->identity_mismatch=!(name_ok&&pair_ok);
    return name_ok&&pair_ok;
}

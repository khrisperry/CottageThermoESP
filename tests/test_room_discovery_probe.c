/* Tests the ACTUAL room mDNS identity probe against a mocked HTTP service.
 * This reproduces v0.13.8's silent 200 OK + ESP_FAIL response truncation:
 * /api/status grew past the original 1800-byte stack buffer in v0.6.4. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "room_discovery_probe.h"
#include "room_transport.h"

static const char *response;
static int response_status=200;
static bool link_online=true;
static unsigned calls;
static size_t capacity_seen;
static bool oversize;
static char *make_status(const char *hostname,const char *mac,size_t filler)
{
    cJSON *root=cJSON_CreateObject();
    cJSON *net=cJSON_AddObjectToObject(root,"network");
    cJSON_AddStringToObject(net,"hostname",hostname);
    cJSON_AddStringToObject(net,"station_mac",mac);
    char *value=malloc(filler+1);assert(value);
    memset(value,'x',filler);value[filler]=0;
    cJSON_AddStringToObject(root,"diagnostics",value);free(value);
    char *body=cJSON_PrintUnformatted(root);cJSON_Delete(root);assert(body);
    return body;
}
esp_err_t room_http(const char *origin,const char *header,const char *token,
                    const char *path,const char *post,void *out,size_t capacity,
                    int timeout_ms,int *status,int *length)
{
    ++calls;
    assert(origin && !strcmp(origin,"http://192.168.60.85"));
    assert(!header&&!token&&!post&&!strcmp(path,"/api/status"));
    assert(timeout_ms>=1800);
    capacity_seen=capacity;
    *status=link_online?response_status:0;
    *length=0;
    if(!link_online)return ESP_FAIL;
    if(!response || response_status!=200)return ESP_FAIL;
    size_t n=strlen(response);
    if(n>=capacity-1){oversize=true;return ESP_FAIL;}
    memcpy(out,response,n+1);*length=(int)n;return ESP_OK;
}
int main(void)
{
    const char *url="http://192.168.60.85";
    room_discovery_result_t r={0};
    char *full=make_status("CottageThermoController","A0:F2:62:F3:F0:E8",2700);
    assert(strlen(full)>1800);
    response=full;
    assert(room_discovery_probe_origin(url,true,"a0:f2:62:f3:f0:e8",&r));
    assert(capacity_seen>=4096 && r.http_error==ESP_OK && r.http_status==200);
    assert(r.response_bytes==(int)strlen(full) && !r.identity_mismatch);
    /* Touchscreen was blocked, while controller and ESP-NOW stayed online.
     * After unblock, the next discovery must succeed without a restart. */
    link_online=false;
    assert(!room_discovery_probe_origin(url,true,"A0:F2:62:F3:F0:E8",&r));
    assert(r.http_error==ESP_FAIL);
    link_online=true;
    assert(room_discovery_probe_origin(url,true,"A0:F2:62:F3:F0:E8",&r));
    /* Do not accept another controller on the same mDNS/IP network. */
    assert(!room_discovery_probe_origin(url,true,"AA:BB:CC:DD:EE:FF",&r));
    assert(r.identity_mismatch);
    /* When radio is intentionally disabled, preserve hostname verification. */
    assert(room_discovery_probe_origin(url,false,NULL,&r));
    free(full);
    full=make_status("NotOurController","A0:F2:62:F3:F0:E8",0);
    response=full;
    assert(!room_discovery_probe_origin(url,false,NULL,&r));
    assert(r.identity_mismatch);free(full);
    response="{bad";
    assert(!room_discovery_probe_origin(url,true,"A0:F2:62:F3:F0:E8",&r));
    assert(r.identity_mismatch);
    response_status=404;
    assert(!room_discovery_probe_origin(url,true,"A0:F2:62:F3:F0:E8",&r));
    assert(r.http_status==404);
    response_status=200;
    full=make_status("CottageThermoController","A0:F2:62:F3:F0:E8",9000);
    response=full;oversize=false;
    assert(!room_discovery_probe_origin(url,true,"A0:F2:62:F3:F0:E8",&r));
    assert(oversize && r.http_error==ESP_FAIL);
    free(full);
    assert(calls>=9);
    puts("PASS: large HTTP200 diagnostic response, reconnect after Wi-Fi isolation, MAC identity, bounded oversized/malformed replies");
    return 0;
}

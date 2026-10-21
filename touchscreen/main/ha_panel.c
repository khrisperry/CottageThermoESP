/* Local panel API backed by direct ESP-to-Home-Assistant requests. */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "ha_panel.h"
#include "room_ha.h"
#include "esp_heap_caps.h"

extern const unsigned char home_start[] asm("_binary_home_html_start");
extern const unsigned char home_end[] asm("_binary_home_html_end");
static bool authorized(httpd_req_t *r)
{
    char token[96]={0};
    if(strlen(CONFIG_ROOM_API_TOKEN)!=32 || httpd_req_get_hdr_value_str(r,"X-Cottage-Token",token,sizeof(token))!=ESP_OK)return false;
    unsigned diff=strlen(token)^strlen(CONFIG_ROOM_API_TOKEN);
    for(unsigned i=0;i<32;++i)diff|=(unsigned char)token[i]^(unsigned char)CONFIG_ROOM_API_TOKEN[i];
    return diff==0;
}
static esp_err_t fail(httpd_req_t *r,const char *status,const char *message)
{
    httpd_resp_set_status(r,status);httpd_resp_set_type(r,"application/json");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_sendstr(r,message);
}
static esp_err_t home_get(httpd_req_t *r)
{
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,(const char*)home_start,home_end-home_start-1);
}
static esp_err_t upstream(httpd_req_t *r)
{
    if(!authorized(r))return fail(r,"401 Unauthorized","{\"error\":\"Room pairing token required\"}");
    bool camera=!strcmp(r->uri,"/api/ha/camera");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    if(camera){
        uint8_t *data=heap_caps_malloc(ROOM_HA_CAMERA_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!data)return fail(r,"503 Service Unavailable","{\"error\":\"Camera memory unavailable\"}");
        int code,length;esp_err_t err=room_ha_camera(data,ROOM_HA_CAMERA_CAPACITY,&code,&length);
        if(err!=ESP_OK||code!=200||length<2||data[0]!=0xff||data[1]!=0xd8){free(data);return fail(r,"503 Service Unavailable","{\"error\":\"Home Assistant camera unavailable\"}");}
        httpd_resp_set_type(r,"image/jpeg");err=httpd_resp_send(r,(char*)data,length);free(data);return err;
    }
    int code=200;cJSON *result;
    if(r->method==HTTP_POST){
        char payload[384];
        if(r->content_len<=0||r->content_len>=sizeof(payload))return fail(r,"400 Bad Request","{\"error\":\"Invalid command body\"}");
        size_t at=0;while(at<r->content_len){int n=httpd_req_recv(r,payload+at,r->content_len-at);if(n<=0)return ESP_FAIL;at+=n;}payload[at]=0;
        cJSON *p=cJSON_ParseWithLengthOpts(payload,at+1,NULL,true);result=room_ha_command(p,&code);cJSON_Delete(p);
    }else result=room_ha_status();
    char *encoded=cJSON_PrintUnformatted(result);cJSON_Delete(result);
    if(!encoded)return fail(r,"503 Service Unavailable","{\"error\":\"Insufficient memory\"}");
    if(code!=200)httpd_resp_set_status(r,code==409?"409 Conflict":"400 Bad Request");
    httpd_resp_set_type(r,"application/json");esp_err_t err=httpd_resp_sendstr(r,encoded);free(encoded);return err;
}
void ha_panel_register(httpd_handle_t h)
{
    const httpd_uri_t routes[]={ {.uri="/home",.method=HTTP_GET,.handler=home_get},
        {.uri="/api/ha/status",.method=HTTP_GET,.handler=upstream},
        {.uri="/api/ha/command",.method=HTTP_POST,.handler=upstream},
        {.uri="/api/ha/camera",.method=HTTP_GET,.handler=upstream}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i)ESP_ERROR_CHECK(httpd_register_uri_handler(h,&routes[i]));
}

/* Thin client to the local HA bridge. No Home Assistant bearer token on ESP. */
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
    if(!strlen(CONFIG_ROOM_HA_BRIDGE_URL)||strlen(CONFIG_ROOM_HA_BRIDGE_TOKEN)<32)
        return fail(r,"503 Service Unavailable","{\"error\":\"Home Assistant bridge is not configured in menuconfig\"}");
    bool camera=!strcmp(r->uri,"/api/ha/camera");
    bool status=!strcmp(r->uri,"/api/ha/status");
    if(strncmp(CONFIG_ROOM_HA_BRIDGE_URL,"http://",7) && strncmp(CONFIG_ROOM_HA_BRIDGE_URL,"https://",8))
        return fail(r,"503 Service Unavailable","{\"error\":\"Bridge URL must use HTTP or HTTPS\"}");
    if(!strncmp(CONFIG_ROOM_HA_BRIDGE_URL,"https://",8) && time(NULL)<1767225600)
        return fail(r,"503 Service Unavailable","{\"error\":\"Waiting for network time before verified HTTPS\"}");
    char url[320],payload[384];const char *post=NULL;
    if(snprintf(url,sizeof(url),"%s%s",CONFIG_ROOM_HA_BRIDGE_URL,r->uri)>=(int)sizeof(url))
        return fail(r,"503 Service Unavailable","{\"error\":\"Bridge URL too long\"}");
    if(r->method==HTTP_POST) {
        if(r->content_len<=0 || r->content_len>=sizeof(payload))return fail(r,"400 Bad Request","{\"error\":\"Invalid command body\"}");
        size_t nread=0;
        while(nread<r->content_len){int n=httpd_req_recv(r,payload+nread,r->content_len-nread);if(n<=0)return ESP_FAIL;nread+=n;}
        payload[nread]=0;
        cJSON *j=cJSON_ParseWithLengthOpts(payload,nread+1,NULL,true);
        bool valid=cJSON_IsObject(j);cJSON_Delete(j);
        if(!valid)return fail(r,"400 Bad Request","{\"error\":\"JSON object required\"}");
        post=payload;
    }
    size_t cap=camera?96001:8192;
    char *data=malloc(cap);
    if(!data)return fail(r,"503 Service Unavailable","{\"error\":\"Insufficient memory for bridge response\"}");
    esp_http_client_config_t cfg={.url=url,.timeout_ms=camera?7000:(post?20000:3000),.disable_auto_redirect=true,
        .crt_bundle_attach=esp_crt_bundle_attach};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);
    if(!h){free(data);return fail(r,"503 Service Unavailable","{\"error\":\"Bridge client unavailable\"}");}
    esp_http_client_set_header(h,"X-Cottage-Token",CONFIG_ROOM_HA_BRIDGE_TOKEN);
    if(post){esp_http_client_set_method(h,HTTP_METHOD_POST);esp_http_client_set_header(h,"Content-Type","application/json");}
    esp_err_t err=esp_http_client_open(h,post?strlen(post):0);
    if(err==ESP_OK && post) {
        int at=0,len=strlen(post);
        while(at<len){int n=esp_http_client_write(h,post+at,len-at);if(n<=0){err=ESP_FAIL;break;}at+=n;}
    }
    if(err==ESP_OK && esp_http_client_fetch_headers(h)<0)err=ESP_FAIL;
    int code=esp_http_client_get_status_code(h),size=-1;
    if(err==ESP_OK){size=esp_http_client_read_response(h,data,cap-1);if(size<0 || size>=(int)cap-1 || !esp_http_client_is_complete_data_received(h))err=ESP_FAIL;else data[size]=0;}
    esp_http_client_close(h);esp_http_client_cleanup(h);
    if(err!=ESP_OK){free(data);return fail(r,"503 Service Unavailable","{\"error\":\"Bridge unavailable; no command retry queued. Check device state before retrying.\"}");}
    if(code!=200) {
        // Return bridge's bounded JSON error, not its HTTP headers or credentials.
        cJSON *j=cJSON_Parse(data);bool json=cJSON_IsObject(j);cJSON_Delete(j);
        httpd_resp_set_status(r,code==401?"502 Bad Gateway":code==409?"409 Conflict":code==400?"400 Bad Request":"503 Service Unavailable");
        httpd_resp_set_type(r,"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
        err=httpd_resp_sendstr(r,json?data:"{\"error\":\"Bridge rejected request\"}");free(data);return err;
    }
    if(status) {
        cJSON *j=cJSON_Parse(data);
        if(!cJSON_IsObject(j)){cJSON_Delete(j);free(data);return fail(r,"502 Bad Gateway","{\"error\":\"Invalid bridge status\"}");}
        cJSON_AddBoolToObject(j,"room_client",true);char *encoded=cJSON_PrintUnformatted(j);cJSON_Delete(j);free(data);data=encoded;
        if(!data)return fail(r,"503 Service Unavailable","{\"error\":\"Insufficient memory\"}");
        size=strlen(data);
    }
    httpd_resp_set_type(r,camera?"image/jpeg":"application/json");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    err=httpd_resp_send(r,data,size);free(data);return err;
}
void ha_panel_register(httpd_handle_t h)
{
    const httpd_uri_t routes[]={ {.uri="/home",.method=HTTP_GET,.handler=home_get},
        {.uri="/api/ha/status",.method=HTTP_GET,.handler=upstream},
        {.uri="/api/ha/command",.method=HTTP_POST,.handler=upstream},
        {.uri="/api/ha/camera",.method=HTTP_GET,.handler=upstream}};
    for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i)ESP_ERROR_CHECK(httpd_register_uri_handler(h,&routes[i]));
}

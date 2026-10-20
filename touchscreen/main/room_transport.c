#include "room_transport.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <time.h>
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

esp_err_t room_http(const char *origin, const char *header, const char *token,
                    const char *path, const char *post, void *out, size_t capacity,
                    int timeout_ms, int *status, int *length)
{
    *status=0; *length=0;
    if (!origin || !token || !*token || capacity<2) return ESP_ERR_INVALID_ARG;
    bool tls=!strncmp(origin,"https://",8);
    if (!tls && strncmp(origin,"http://",7)) return ESP_ERR_INVALID_ARG;
    const char *host=origin+(tls?8:7);
    /* Configured origin only: no embedded credentials, query strings or paths. */
    if (!*host || strpbrk(host,"/@?#\\ \r\n")) return ESP_ERR_INVALID_ARG;
    if (tls && time(NULL)<1767225600) return ESP_ERR_INVALID_STATE;
    char url[320];
    if (snprintf(url,sizeof(url),"%s%s",origin,path)>=(int)sizeof(url)) return ESP_ERR_INVALID_SIZE;
    esp_http_client_config_t cfg={.url=url,.timeout_ms=timeout_ms,
        .disable_auto_redirect=true,.crt_bundle_attach=esp_crt_bundle_attach};
    esp_http_client_handle_t h=esp_http_client_init(&cfg);
    if (!h) return ESP_ERR_NO_MEM;
    esp_http_client_set_header(h,header,token);
    if (post) {
        esp_http_client_set_method(h,HTTP_METHOD_POST);
        esp_http_client_set_header(h,"Content-Type","application/json");
    }
    esp_err_t err=esp_http_client_open(h,post?strlen(post):0);
    if (err==ESP_OK && post) {
        int at=0,len=strlen(post);
        while (at<len) { int n=esp_http_client_write(h,post+at,len-at);
            if (n<=0) {err=ESP_FAIL;break;} at+=n; }
    }
    if (err==ESP_OK && esp_http_client_fetch_headers(h)<0) err=ESP_FAIL;
    *status=esp_http_client_get_status_code(h);
    if (err==ESP_OK) {
        int n=esp_http_client_read_response(h,out,capacity-1);
        if (n<0 || n>=(int)capacity-1 || !esp_http_client_is_complete_data_received(h)) err=ESP_FAIL;
        else { ((char*)out)[n]=0; *length=n; }
    }
    esp_http_client_close(h); esp_http_client_cleanup(h);
    return err;
}

/* perform() resets the HTTP parser and keeps a fully consumed connection alive.
 * The streaming open() API used above starts a new connection in IDF 6.0.1. */
struct room_http_session {
    esp_http_client_handle_t client;
    char origin[256];
};
typedef struct {char *out;size_t capacity,used;bool overflow;} response_t;
static esp_err_t receive_response(esp_http_client_event_t *event)
{
    response_t *r=event->user_data;
    if(event->event_id==HTTP_EVENT_ON_DATA&&r&&event->data_len>0){
        size_t n=(size_t)event->data_len;
        if(n>r->capacity-1-r->used)r->overflow=true;
        else if(!r->overflow){memcpy(r->out+r->used,event->data,n);r->used+=n;}
    }
    return ESP_OK;
}
esp_err_t room_http_keepalive(room_http_session_t **session,
                    const char *origin,const char *header,const char *token,
                    const char *path,const char *post,void *out,size_t capacity,
                    int timeout_ms,int *status,int *length)
{
    *status=0;*length=0;
    if(!session||!origin||!header||!token||!*token||!path||path[0]!='/'||path[1]=='/'||
       strpbrk(path,"\r\n")||!out||capacity<2||capacity>INT_MAX)return ESP_ERR_INVALID_ARG;
    if(strncmp(origin,"https://",8)||!origin[8]||strpbrk(origin+8,"/@?#\\ \r\n")||
       strlen(origin)>=sizeof(((room_http_session_t*)0)->origin))return ESP_ERR_INVALID_ARG;
    if(time(NULL)<1767225600)return ESP_ERR_INVALID_STATE;
    if(*session&&strcmp((*session)->origin,origin))return ESP_ERR_INVALID_ARG;
    char url[320];int size=snprintf(url,sizeof(url),"%s%s",origin,path);
    if(size<0||size>=(int)sizeof(url))return ESP_ERR_INVALID_SIZE;
    if(!*session){
        *session=calloc(1,sizeof(**session));if(!*session)return ESP_ERR_NO_MEM;
        snprintf((*session)->origin,sizeof((*session)->origin),"%s",origin);
    }
    room_http_session_t *s=*session;
    response_t response={.out=out,.capacity=capacity};
    if(!s->client){
        esp_http_client_config_t cfg={.url=url,.timeout_ms=timeout_ms,
            .disable_auto_redirect=true,.crt_bundle_attach=esp_crt_bundle_attach,
            .event_handler=receive_response,.keep_alive_enable=true,
            .max_authorization_retries=-1};
        s->client=esp_http_client_init(&cfg);if(!s->client)return ESP_ERR_NO_MEM;
    }
    esp_http_client_handle_t h=s->client;
    esp_err_t err=esp_http_client_set_url(h,url);
    if(err==ESP_OK)err=esp_http_client_set_timeout_ms(h,timeout_ms);
    if(err==ESP_OK)err=esp_http_client_set_header(h,header,token);
    if(err==ESP_OK)err=esp_http_client_set_method(h,post?HTTP_METHOD_POST:HTTP_METHOD_GET);
    if(err==ESP_OK)err=esp_http_client_set_post_field(h,post,post?strlen(post):0);
    if(err==ESP_OK)err=esp_http_client_set_header(h,"Content-Type","application/json");
    if(err==ESP_OK)err=esp_http_client_set_user_data(h,&response);
    if(err==ESP_OK){
        /* Exactly one attempt, including POST on a dead keep-alive connection. */
        err=esp_http_client_perform(h);
        *status=esp_http_client_get_status_code(h);
        if(err==ESP_OK&&(response.overflow||!esp_http_client_is_complete_data_received(h)))err=ESP_FAIL;
    }
    esp_http_client_set_user_data(h,NULL);
    if(err==ESP_OK){response.out[response.used]=0;*length=response.used;}
    else{
        /* Drop failed TLS/parser state. Only a later request can reconnect. */
        esp_http_client_cleanup(h);s->client=NULL;
    }
    return err;
}

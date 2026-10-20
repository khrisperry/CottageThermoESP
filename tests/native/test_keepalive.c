/* Production transport against a fake IDF HTTP client: no network or secrets. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "esp_http_client.h"
struct fake_client {
    esp_err_t (*handler)(esp_http_client_event_t *);
    void *user;
    esp_http_client_method_t method;
    const char *body;
    int body_len,status;
    char url[320];
};
static struct fake_client client;
static int inits,attempts,cleanups,fail_next,status_next=200,streaming_calls;
static bool complete=true;
static const char *reply="{}";
static time_t clock_value=1800000000;
static time_t test_time(time_t *p){if(p)*p=clock_value;return clock_value;}
#define time test_time
#include "../../touchscreen/main/room_transport.c"
#undef time
esp_err_t esp_crt_bundle_attach(void *p){return ESP_OK;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg)
{
    ++inits;memset(&client,0,sizeof(client));
    assert(cfg->disable_auto_redirect&&cfg->keep_alive_enable);
    assert(cfg->max_authorization_retries==-1);
    assert(cfg->crt_bundle_attach==esp_crt_bundle_attach);
    client.handler=cfg->event_handler;return &client;
}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h){++cleanups;return ESP_OK;}
esp_err_t esp_http_client_set_url(esp_http_client_handle_t h,const char *url){snprintf(h->url,sizeof(h->url),"%s",url);return ESP_OK;}
esp_err_t esp_http_client_set_timeout_ms(esp_http_client_handle_t h,int timeout){assert(timeout>0);return ESP_OK;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h,const char *key,const char *value)
{if(!strcmp(key,"Authorization"))assert(!strcmp(value,"Bearer fixture"));return ESP_OK;}
esp_err_t esp_http_client_set_method(esp_http_client_handle_t h,esp_http_client_method_t method){h->method=method;return ESP_OK;}
esp_err_t esp_http_client_set_post_field(esp_http_client_handle_t h,const char *body,int len){h->body=body;h->body_len=len;return ESP_OK;}
esp_err_t esp_http_client_set_user_data(esp_http_client_handle_t h,void *p){h->user=p;return ESP_OK;}
esp_err_t esp_http_client_perform(esp_http_client_handle_t h)
{
    ++attempts;h->status=status_next;
    if(fail_next){fail_next=0;return ESP_FAIL;}
    size_t n=strlen(reply),half=n/2;
    esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_DATA,.user_data=h->user,.data=(void*)reply,.data_len=half};
    h->handler(&e);e.data=(void*)(reply+half);e.data_len=n-half;h->handler(&e);
    return ESP_OK;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h){return h->status;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h){return complete;}
/* Any accidental return to the old streaming API fails this regression. */
esp_err_t esp_http_client_open(esp_http_client_handle_t h,int n){++streaming_calls;return ESP_FAIL;}
int esp_http_client_write(esp_http_client_handle_t h,const char *p,int n){assert(0);return -1;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h){assert(0);return -1;}
int esp_http_client_read_response(esp_http_client_handle_t h,char *p,int n){assert(0);return -1;}
esp_err_t esp_http_client_close(esp_http_client_handle_t h){assert(0);return ESP_FAIL;}
static esp_err_t request(room_http_session_t **s,const char *path,const char *post,char *out,size_t capacity,int *code,int *n)
{
    return room_http_keepalive(s,"https://ha.test","Authorization","Bearer fixture",path,post,out,capacity,1000,code,n);
}
int main(void)
{
    room_http_session_t *s=NULL;char out[32];int code,n;
    for(int k=0;k<11;++k)assert(request(&s,"/api/states/light.fixture",NULL,out,sizeof(out),&code,&n)==ESP_OK);
    assert(inits==1&&attempts==11&&cleanups==0&&streaming_calls==0);
    assert(code==200&&n==2&&!strcmp(out,"{}")&&client.user==NULL);
    assert(request(&s,"/api/services/light/turn_on","{}",out,sizeof(out),&code,&n)==ESP_OK);
    assert(client.method==HTTP_METHOD_POST&&client.body_len==2&&inits==1);
    assert(request(&s,"/api/states/light.fixture",NULL,out,sizeof(out),&code,&n)==ESP_OK);
    assert(client.method==HTTP_METHOD_GET&&client.body==NULL&&client.body_len==0);
    fail_next=1;int before=attempts;
    assert(request(&s,"/api/services/light/turn_on","{}",out,sizeof(out),&code,&n)==ESP_FAIL);
    assert(attempts==before+1&&cleanups==1&&s->client==NULL&&n==0);
    assert(request(&s,"/api/states/light.fixture",NULL,out,sizeof(out),&code,&n)==ESP_OK);
    assert(inits==2);
    reply="response too large";assert(request(&s,"/api/states/light.fixture",NULL,out,4,&code,&n)==ESP_FAIL);
    assert(n==0&&s->client==NULL);reply="{}";
    complete=false;assert(request(&s,"/api/states/light.fixture",NULL,out,sizeof(out),&code,&n)==ESP_FAIL);complete=true;
    status_next=302;before=attempts;
    assert(request(&s,"/redirect",NULL,out,sizeof(out),&code,&n)==ESP_OK&&code==302&&attempts==before+1);
    status_next=401;before=attempts;
    assert(request(&s,"/unauthorized",NULL,out,sizeof(out),&code,&n)==ESP_OK&&code==401&&attempts==before+1);
    before=attempts;
    assert(room_http_keepalive(&s,"https://other.test","Authorization","Bearer fixture","/",NULL,out,sizeof(out),1000,&code,&n)==ESP_ERR_INVALID_ARG);
    assert(request(&s,"//other.test",NULL,out,sizeof(out),&code,&n)==ESP_ERR_INVALID_ARG);
    clock_value=0;assert(request(&s,"/",NULL,out,sizeof(out),&code,&n)==ESP_ERR_INVALID_STATE);
    assert(attempts==before);
    esp_http_client_cleanup(s->client);free(s);
    puts("PASS: persistent session reuse, POST/GET reset, bounded reads, failure cleanup, no POST replay, redirects/auth and origin/time guards");
    return 0;
}

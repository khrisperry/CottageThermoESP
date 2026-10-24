#pragma once
#include "host.h"
typedef struct {const char *name,*value;} test_header_t;
typedef struct httpd_req {
 int content_len,at,fail_at;const char *body;test_header_t headers[8];
 int code;char response[16384];
} httpd_req_t;
typedef void *httpd_handle_t;
typedef struct {const char *uri;int method;int(*handler)(httpd_req_t*);} httpd_uri_t;
#define HTTP_GET 0
#define HTTP_POST 1
int httpd_req_get_hdr_value_str(httpd_req_t*,const char*,char*,size_t);
size_t httpd_req_get_hdr_value_len(httpd_req_t*,const char*);
int httpd_req_recv(httpd_req_t*,char*,size_t);
int httpd_resp_set_status(httpd_req_t*,const char*);
int httpd_resp_set_type(httpd_req_t*,const char*);
int httpd_resp_set_hdr(httpd_req_t*,const char*,const char*);
int httpd_resp_sendstr(httpd_req_t*,const char*);
int httpd_resp_send(httpd_req_t*,const char*,int);
int httpd_register_uri_handler(void*,const httpd_uri_t*);

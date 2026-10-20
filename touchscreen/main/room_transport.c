#include "room_transport.h"
#include <stdio.h>
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

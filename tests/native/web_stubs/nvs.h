#pragma once
#include "host.h"
typedef unsigned nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
int nvs_open(const char*,int,nvs_handle_t*);
void nvs_close(nvs_handle_t);
int nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
int nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
int nvs_get_str(nvs_handle_t,const char*,char*,size_t*);
int nvs_set_str(nvs_handle_t,const char*,const char*);
int nvs_commit(nvs_handle_t);

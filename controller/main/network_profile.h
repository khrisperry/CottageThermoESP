#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
/* Fixed-size NVS record; independent of calibration and bench settings. */
#define NETWORK_PROFILE_MAGIC 0x43574e31U
typedef struct {uint32_t magic;char ssid[33],password[65];} network_profile_t;
static inline bool network_profile_valid(const network_profile_t *p)
{
    if(p->magic!=NETWORK_PROFILE_MAGIC||!memchr(p->ssid,0,sizeof(p->ssid))||
       !memchr(p->password,0,sizeof(p->password)))return false;
    size_t n=strlen(p->password);
    if(!*p->ssid)return n==0; /* Explicit AP-only profile. */
    if(n==0)return true; /* Open LAN. */
    if(n<8||n>64)return false;
    for(size_t i=0;i<n;++i){
        unsigned char c=p->password[i];
        if(c<32||c>126)return false;
        if(n==64&&!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
    }
    return true;
}
static inline bool network_profile_patch(const network_profile_t *current,
    const char *ssid,const char *password,bool open,network_profile_t *next)
{
    if(strlen(ssid)>32||strlen(password)>64)return false;
    *next=*current;
    memset(next->ssid,0,sizeof(next->ssid));memcpy(next->ssid,ssid,strlen(ssid));
    if(!*ssid||open)memset(next->password,0,sizeof(next->password));
    else if(*password){
        memset(next->password,0,sizeof(next->password));
        memcpy(next->password,password,strlen(password));
    }else if(strcmp(ssid,current->ssid)||!*current->password)return false;
    return network_profile_valid(next);
}

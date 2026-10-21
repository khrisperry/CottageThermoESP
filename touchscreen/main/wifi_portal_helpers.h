#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
/* Hardware-independent validation and DNS parsing shared with host tests. */
static inline bool portal_profile_valid(const char *ssid,const char *password)
{
    size_t s=strlen(ssid),p=strlen(password);
    if(!s||s>32||p>64||(p>0&&p<8))return false;
    for(size_t n=0;n<s;++n)if((unsigned char)ssid[n]<32||(unsigned char)ssid[n]==127)return false;
    for(size_t n=0;n<p;++n){
        unsigned char c=(unsigned char)password[n];
        if(p==64){if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
        else if(c<32||c>126)return false;
    }
    return true;
}
static inline size_t portal_dns_reply(uint8_t *packet,size_t length,size_t capacity)
{
    if(length<17||length>capacity||(packet[2]&0xf8)||packet[4]!=0||packet[5]!=1)return 0;
    size_t at=12;
    while(at<length&&packet[at]){
        unsigned label=packet[at++];
        if(label>63||at+label>=length)return 0;
        at+=label;
    }
    if(at>=length||at-12>254||at+5>length)return 0;
    ++at;
    unsigned type=(packet[at]<<8)|packet[at+1],cls=(packet[at+2]<<8)|packet[at+3];
    size_t end=at+4;
    bool answer=type==1&&cls==1;
    if(answer&&end+16>capacity)return 0;
    packet[2]=0x81;packet[3]=0x80;packet[6]=0;packet[7]=answer?1:0;
    packet[8]=packet[9]=packet[10]=packet[11]=0;
    if(answer){
        const uint8_t a[16]={0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,192,168,4,1};
        memcpy(packet+end,a,sizeof(a));end+=sizeof(a);
    }
    return end;
}

/* The HTTP server uses IPv4-mapped IPv6 sockets when LWIP_IPV6 is enabled. */
static inline bool portal_setup_address(const uint8_t *bytes,size_t size)
{
    static const uint8_t ap[4]={192,168,4,1};
    if(size==4)return !memcmp(bytes,ap,sizeof(ap));
    if(size!=16)return false;
    for(size_t n=0;n<10;++n)if(bytes[n])return false;
    return bytes[10]==0xff&&bytes[11]==0xff&&!memcmp(bytes+12,ap,sizeof(ap));
}

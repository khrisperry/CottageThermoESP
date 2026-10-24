#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

/* Both boards are little-endian ESP32-S3. Version and exact length are checked
 * before accessing a packet. No pointers, credentials or raw PWM travel here. */
#define TL_MAGIC 0x434c4e4bU
#define TL_VERSION 1
#define TL_LEASE_MS 5000U
enum { TL_STATUS=1, TL_TARGET, TL_SAMPLE, TL_REPLY };
enum { TL_OK=0, TL_INVALID, TL_EXPIRED, TL_REPLAY, TL_SAVE_FAILED };
enum { TL_HAS_TEMP=1, TL_SIMULATION=2, TL_HAS_TARGET=4, TL_MQTT_CONFIGURED=8, TL_MQTT_ONLINE=16 };
typedef struct {
    uint64_t client, sequence, controller_boot, deadline_ms;
    uint32_t magic, age_ms;
    float value, temperature, target;
    uint8_t version, operation, result, flags, mode;
    uint8_t reserved[7];
    char reason[48];
} tl_packet_t;
_Static_assert(sizeof(tl_packet_t)==112,"ESP-NOW protocol layout changed");
_Static_assert(sizeof(tl_packet_t)<=250,"Keep v1 ESP-NOW compatibility");
typedef struct { uint64_t client, sequence, enrolled_at; } tl_guard_t;
static inline tl_packet_t tl_request(uint8_t operation,uint64_t client,uint64_t sequence)
{
    tl_packet_t p={0};p.magic=TL_MAGIC;p.version=TL_VERSION;
    p.operation=operation;p.client=client;p.sequence=sequence;return p;
}
static inline bool tl_valid(const tl_packet_t *p)
{
    if(p->magic!=TL_MAGIC||p->version!=TL_VERSION||!p->client||!p->sequence||
       p->operation<TL_STATUS||p->operation>TL_REPLY)return false;
    for(size_t i=0;i<sizeof(p->reserved);++i)if(p->reserved[i])return false;
    return memchr(p->reason,0,sizeof(p->reason))!=NULL;
}
/* Caller serializes HTTP and radio operations using the same controller lock.
 * One configured panel identity per boot. An unfamiliar client can replace it
 * only via a fresh status handshake; mutations never enroll a client. */
static inline int tl_check(tl_guard_t *g,const tl_packet_t *p,uint64_t boot,uint64_t now)
{
    if(!tl_valid(p)||p->operation==TL_REPLY)return TL_INVALID;
    if(p->operation==TL_STATUS){
        if(g->client!=p->client){
            if(g->client&&now-g->enrolled_at<TL_LEASE_MS)return TL_REPLAY;
            memset(g,0,sizeof(*g));g->client=p->client;g->enrolled_at=now;
        }
        return TL_OK;
    }
    if(p->controller_boot!=boot||p->deadline_ms<=now||p->deadline_ms-now>TL_LEASE_MS)return TL_EXPIRED;
    if(g->client!=p->client||p->sequence<=g->sequence)return TL_REPLAY;
    if(!isfinite(p->value)||(p->operation==TL_TARGET?(p->value<45||p->value>80):
        (p->value<32||p->value>110||p->age_ms>2000)))return TL_INVALID;
    return TL_OK;
}
static inline void tl_hex(const tl_packet_t *p,char out[sizeof(tl_packet_t)*2+1])
{
    const char *hex="0123456789abcdef";const uint8_t *b=(const uint8_t*)p;
    for(size_t i=0;i<sizeof(*p);++i){out[i*2]=hex[b[i]>>4];out[i*2+1]=hex[b[i]&15];}out[sizeof(*p)*2]=0;
}
static inline int tl_digit(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;}
static inline bool tl_unhex(const char *s,tl_packet_t *p)
{
    if(!s||strlen(s)!=sizeof(*p)*2)return false;
    uint8_t *b=(uint8_t*)p;
    for(size_t i=0;i<sizeof(*p);++i){int a=tl_digit(s[2*i]),c=tl_digit(s[2*i+1]);if(a<0||c<0)return false;b[i]=(a<<4)|c;}
    return tl_valid(p);
}

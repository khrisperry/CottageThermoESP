#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
typedef struct {
    bool mqtt_enabled,radio_enabled;
    char broker[256],username[96],password[128],prefix[64],name[64];
    char peer[18],key[33];
} cottage_integration_t;
static inline bool cottage_integration_terminated(const cottage_integration_t *p)
{
    return memchr(p->broker,0,sizeof(p->broker))&&memchr(p->username,0,sizeof(p->username))&&
        memchr(p->password,0,sizeof(p->password))&&memchr(p->prefix,0,sizeof(p->prefix))&&
        memchr(p->name,0,sizeof(p->name))&&memchr(p->peer,0,sizeof(p->peer))&&memchr(p->key,0,sizeof(p->key));
}
static inline bool cottage_origin(char *out,size_t cap,const char *input,bool https_only)
{
    if(!input)return false;
    size_t n=strlen(input);
    if(!n||n>=cap)return false;
    char s[256];if(n>=sizeof(s))return false;memcpy(s,input,n+1);
    while(n&&s[n-1]=='/')s[--n]=0;
    const char *host=s;const char *prefix="";
    if(!strncmp(host,"https://",8))host+=8;
    else if(!https_only&&!strncmp(host,"http://",7))host+=7;
    else if(https_only)return false;
    else prefix="http://";
    if(!*host||strpbrk(host,"/@?#\\ \r\n\t")||strchr(host,':')!=strrchr(host,':'))return false;
    int written=snprintf(out,cap,"%s%s",prefix,s);return written>0&&(size_t)written<cap;
}
static inline bool cottage_string(cJSON *j,const char *key,char *out,size_t cap,bool optional)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!v)return optional;
    if(!cJSON_IsString(v)||strlen(v->valuestring)>=cap)return false;
    for(const unsigned char *p=(const unsigned char*)v->valuestring;*p;++p)if(*p<32||*p==127)return false;
    snprintf(out,cap,"%s",v->valuestring);return true;
}
static inline bool cottage_bool(cJSON *j,const char *key,bool *out)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsBool(v))return false;*out=cJSON_IsTrue(v);return true;
}
static inline bool cottage_radio_valid(const cottage_integration_t *p)
{
    if(!p->radio_enabled)return true;
    if(strlen(p->peer)!=17||strlen(p->key)!=32)return false;
    unsigned first=0,combined=0;
    for(unsigned i=0;i<17;++i){char c=p->peer[i];if(i%3==2){if(c!=':')return false;}else{
        unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;
        if(d>15)return false;
        if(i<2)first=(first<<4)|d;
        combined|=d;
    }}
    if((first&1)||!combined)return false;
    for(unsigned i=0;i<32;++i){char c=p->key[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
    return true;
}
/* Patch one integration at a time. Secret omissions retain existing values;
 * explicit clear flags remove them. Failed validation never changes current. */
static inline bool cottage_integration_patch(cottage_integration_t *p,cJSON *j,bool radio)
{
    if(!cJSON_IsObject(j))return false;
    cottage_integration_t next=*p;bool clear=false;
    if(radio){
        if(!cottage_bool(j,"enabled",&next.radio_enabled)||!cottage_string(j,"peer",next.peer,sizeof(next.peer),false))return false;
        cJSON *key=cJSON_GetObjectItemCaseSensitive(j,"key");
        if(key&&(!cJSON_IsString(key)||strlen(key->valuestring)>=sizeof(next.key)))return false;
        if(key&&*key->valuestring)snprintf(next.key,sizeof(next.key),"%s",key->valuestring);
        if(!cottage_radio_valid(&next))return false;
    }else{
        if(!cottage_bool(j,"enabled",&next.mqtt_enabled)||
           !cottage_string(j,"broker",next.broker,sizeof(next.broker),false)||
           !cottage_string(j,"username",next.username,sizeof(next.username),false)||
           !cottage_string(j,"prefix",next.prefix,sizeof(next.prefix),false)||
           !cottage_string(j,"name",next.name,sizeof(next.name),false))return false;
        cJSON *password=cJSON_GetObjectItemCaseSensitive(j,"password");
        if(password&&(!cJSON_IsString(password)||strlen(password->valuestring)>=sizeof(next.password)))return false;
        if(cJSON_HasObjectItem(j,"clear_password")&&!cottage_bool(j,"clear_password",&clear))return false;
        if(clear&&password&&*password->valuestring)return false;
        if(clear)next.password[0]=0;
        else if(password&&*password->valuestring){if(!cottage_string(j,"password",next.password,sizeof(next.password),false))return false;}
        if(!*next.prefix||next.prefix[0]=='/'||next.prefix[strlen(next.prefix)-1]=='/'||strstr(next.prefix,"//")||!*next.name||strspn(next.prefix,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- /")!=strlen(next.prefix)||strchr(next.prefix,' '))return false;
        if(next.mqtt_enabled){
            const char *host=!strncmp(next.broker,"mqtts://",8)?next.broker+8:!strncmp(next.broker,"mqtt://",7)?next.broker+7:NULL;
            if(!host||!*host||strpbrk(host,"/@?#\\ \r\n\t"))return false;
            const char *colon=strchr(host,':');
            if(colon){const char *port=colon+1;if(colon==host||colon!=strrchr(host,':')||!*port||strspn(port,"0123456789")!=strlen(port))return false;
                unsigned long number=strtoul(port,NULL,10);if(number<1||number>65535)return false;}

        }
    }
    *p=next;return true;
}

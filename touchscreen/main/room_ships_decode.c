#include "room_ships.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool num(cJSON *j,const char *key,double lo,double hi,double *out)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble)||v->valuedouble<lo||v->valuedouble>hi)return false;
    *out=v->valuedouble;return true;
}
static void text(cJSON *j,const char *key,char *out,size_t cap)
{
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);const char *s=cJSON_IsString(v)?v->valuestring:"";
    size_t used=0;
    while(*s&&used+1<cap){unsigned char lead=(unsigned char)*s;size_t n=lead<0x80?1:(lead&0xe0)==0xc0?2:(lead&0xf0)==0xe0?3:(lead&0xf8)==0xf0?4:0;
        bool valid=n!=0;
        for(size_t i=1;valid&&i<n;++i)if(!s[i]||((unsigned char)s[i]&0xc0)!=0x80)valid=false;
        if(!valid){out[used++]='?';++s;continue;}
        if(used+n>=cap)break;
        if(n==1&&lead<32)out[used++]=' ';else{memcpy(out+used,s,n);used+=n;}
        s+=n;
    }
    out[used]=0;
}
bool room_ships_decode(const char *body,size_t length,room_ships_t *out)
{
    if(!body||!length||length>ROOM_SHIP_BODY_MAX)return false;
    unsigned depth=0;bool quoted=false,escape=false;
    for(size_t i=0;i<length;++i){char c=body[i];
        if(quoted){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
        else if(c=='"')quoted=true;
        else if(c=='{'||c=='['){if(++depth>16)return false;}
        else if(c=='}'||c==']'){if(!depth)return false;--depth;}
    }
    if(depth||quoted)return false;
    const char *end=NULL;cJSON *j=cJSON_ParseWithLengthOpts(body,length,&end,false);
    if(!j)return false;
    while(end<body+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))++end;
    if(end!=body+length){cJSON_Delete(j);return false;}
    double v=0;
    cJSON *list=cJSON_GetObjectItemCaseSensitive(j,"vessels");
    room_ships_t result={0};bool ok=num(j,"schemaVersion",1,1,&v)&&cJSON_IsArray(list)&&cJSON_GetArraySize(list)<=ROOM_SHIP_LIMIT;
    if(ok){ok=num(j,"referenceLatitude",-90,90,&v);result.latitude=v;}
    if(ok){ok=num(j,"referenceLongitude",-180,180,&v);result.longitude=v;}
    if(ok){ok=num(j,"rangeMiles",5,100,&v);result.range=v;}
    if(ok){ok=num(j,"nearbyCount",0,10000,&v)&&floor(v)==v&&v>=cJSON_GetArraySize(list);result.nearby=v;}
    cJSON *entry;
    if(ok)cJSON_ArrayForEach(entry,list){room_ship_t *s=&result.ships[result.count];
        if(!num(entry,"mmsi",100000000,999999999,&v)||floor(v)!=v){ok=false;break;}s->mmsi=v;
        if(!num(entry,"latitude",-90,90,&v)){ok=false;break;}s->lat=v;
        if(!num(entry,"longitude",-180,180,&v)){ok=false;break;}s->lon=v;
        if(!num(entry,"distanceMiles",0,result.range,&v)){ok=false;break;}s->distance=v;
        if(!num(entry,"positionAgeSeconds",0,3600,&v)){ok=false;break;}s->age=v;s->stale=s->age>600;
        text(entry,"name",s->name,sizeof(s->name));if(!*s->name)snprintf(s->name,sizeof(s->name),"MMSI %u",(unsigned)s->mmsi);
        text(entry,"shipType",s->kind,sizeof(s->kind));text(entry,"destination",s->destination,sizeof(s->destination));
        s->speed_valid=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"speedValid"))&&num(entry,"speedKnots",0,102.3,&v);if(s->speed_valid)s->speed=v;
        s->course_valid=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(entry,"courseValid"))&&num(entry,"courseDegrees",0,359.999,&v);if(s->course_valid)s->course=v;
        ++result.count;
    }
    if(ok){result.service=true;result.feed=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"aisConnected"));*out=result;}
    cJSON_Delete(j);return ok;
}

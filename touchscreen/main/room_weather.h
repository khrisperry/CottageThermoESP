#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

enum {RW_TEMP=1,RW_APPARENT=2,RW_HUMIDITY=4,RW_CLOUD=8,RW_WIND=16,RW_GUST=32};
typedef struct {
    bool available;
    unsigned fields;
    char condition[32],reported[40];
    double temperature_f,apparent_f,humidity,cloud_coverage,wind_mph,gust_mph;
} room_weather_t;
static inline const char *rw_text(const cJSON *j,const char *key)
{const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);return cJSON_IsString(v)?v->valuestring:"";}
static inline bool rw_number(const cJSON *j,const char *key,double *out)
{const cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;*out=v->valuedouble;return true;}
static inline bool rw_temperature(const cJSON *a,const char *key,const char *unit,double *out)
{
    double v;if(!rw_number(a,key,&v))return false;
    if(!strcmp(unit,"°C")||!strcmp(unit,"C"))v=v*1.8+32;
    else if(strcmp(unit,"°F")&&strcmp(unit,"F"))return false;
    if(v< -150||v>160)return false;
    *out=v;return true;
}
static inline bool rw_wind(const cJSON *a,const char *key,const char *unit,double *out)
{
    double v;if(!rw_number(a,key,&v)||v<0)return false;
    if(!strcmp(unit,"km/h"))v*=0.621371192;
    else if(!strcmp(unit,"m/s"))v*=2.236936292;
    else if(!strcmp(unit,"kn"))v*=1.150779448;
    else if(!strcmp(unit,"ft/s"))v*=0.681818182;
    else if(strcmp(unit,"mph"))return false;
    if(v>350)return false;
    *out=v;return true;
}
/* Whitelist only current observations. Never expose arbitrary attributes,
 * interpret cloudy/sunny as measured solar gain, or authorize servo output. */
static inline room_weather_t room_weather_parse(const cJSON *j,const char *entity)
{
    room_weather_t w={0};
    if(strcmp(rw_text(j,"entity_id"),entity))return w;
    const char *condition=rw_text(j,"state");
    const char *known[]={"clear-night","cloudy","exceptional","fog","hail","lightning",
        "lightning-rainy","partlycloudy","pouring","rainy","snowy","snowy-rainy","sunny","windy","windy-variant"};
    for(unsigned i=0;i<sizeof(known)/sizeof(known[0]);++i)if(!strcmp(condition,known[i]))w.available=true;
    if(!w.available)return w;
    snprintf(w.condition,sizeof(w.condition),"%s",condition);
    const char *reported=rw_text(j,"last_reported");
    if(!*reported)reported=rw_text(j,"last_updated");
    if(strlen(reported)<sizeof(w.reported))snprintf(w.reported,sizeof(w.reported),"%s",reported);
    const cJSON *a=cJSON_GetObjectItemCaseSensitive(j,"attributes");
    const char *temp_unit=rw_text(a,"temperature_unit"),*wind_unit=rw_text(a,"wind_speed_unit");
    if(rw_temperature(a,"temperature",temp_unit,&w.temperature_f))w.fields|=RW_TEMP;
    if(rw_temperature(a,"apparent_temperature",temp_unit,&w.apparent_f))w.fields|=RW_APPARENT;
    if(rw_number(a,"humidity",&w.humidity)&&w.humidity>=0&&w.humidity<=100)w.fields|=RW_HUMIDITY;
    if(rw_number(a,"cloud_coverage",&w.cloud_coverage)&&w.cloud_coverage>=0&&w.cloud_coverage<=100)w.fields|=RW_CLOUD;
    if(rw_wind(a,"wind_speed",wind_unit,&w.wind_mph))w.fields|=RW_WIND;
    if(rw_wind(a,"wind_gust_speed",wind_unit,&w.gust_mph))w.fields|=RW_GUST;
    return w;
}

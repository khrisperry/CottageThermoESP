#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "room_weather.h"
static room_weather_t parse(const char *json)
{cJSON *j=cJSON_Parse(json);assert(j);room_weather_t w=room_weather_parse(j,"weather.forecast_home");cJSON_Delete(j);return w;}
int main(void)
{
    room_weather_t w=parse("{\"entity_id\":\"weather.forecast_home\",\"state\":\"sunny\",\"last_updated\":\"2026-10-28T12:00:00Z\",\"attributes\":{\"temperature\":10,\"apparent_temperature\":8,\"temperature_unit\":\"°C\",\"humidity\":70,\"cloud_coverage\":0,\"wind_speed\":36,\"wind_gust_speed\":54,\"wind_speed_unit\":\"km/h\",\"access_token\":\"do-not-copy\"}}");
    assert(w.available&&w.fields==63&&!strcmp(w.condition,"sunny"));
    assert(w.temperature_f==50&&fabs(w.apparent_f-46.4)<0.0001);
    assert(w.humidity==70&&w.cloud_coverage==0&&fabs(w.wind_mph-22.36936292)<0.0001);
    assert(fabs(w.gust_mph-33.554044368)<0.0001&&!strcmp(w.reported,"2026-10-28T12:00:00Z"));
    w=parse("{\"entity_id\":\"weather.forecast_home\",\"state\":\"cloudy\",\"attributes\":{\"temperature\":40,\"temperature_unit\":\"°F\",\"wind_speed\":10,\"wind_speed_unit\":\"mph\",\"humidity\":0,\"cloud_coverage\":100}}");
    assert(w.available&&w.temperature_f==40&&w.wind_mph==10&&w.humidity==0&&w.cloud_coverage==100);
    w=parse("{\"entity_id\":\"weather.forecast_home\",\"state\":\"rainy\",\"attributes\":{\"temperature\":1,\"temperature_unit\":\"kelvin\",\"wind_speed\":10,\"wind_speed_unit\":\"unknown\",\"humidity\":101,\"cloud_coverage\":-1}}");
    assert(w.available&&w.fields==0);
    w=parse("{\"entity_id\":\"weather.forecast_home\",\"state\":\"unavailable\",\"attributes\":{\"temperature\":50,\"temperature_unit\":\"°F\"}}");assert(!w.available&&w.fields==0);
    w=parse("{\"entity_id\":\"weather.other\",\"state\":\"sunny\"}");assert(!w.available);
    w=parse("{\"entity_id\":\"weather.forecast_home\",\"state\":\"partlycloudy\"}");assert(w.available&&w.fields==0);
    double mph; cJSON *a=cJSON_Parse("{\"wind_speed\":10}");
    assert(rw_wind(a,"wind_speed","m/s",&mph)&&fabs(mph-22.36936292)<0.0001);
    assert(rw_wind(a,"wind_speed","kn",&mph)&&fabs(mph-11.50779448)<0.0001);cJSON_Delete(a);
    puts("PASS: weather identity/conditions, optional attributes, C/F and wind units, zero/missing/invalid data and source timestamp fallback");
}

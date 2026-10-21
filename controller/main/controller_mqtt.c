/* HA is monitoring-only on the wall thermostat. The actuator has no MQTT client. */
#include "controller_mqtt.h"
void controller_mqtt_start(void){}
bool controller_mqtt_configured(void){return false;}
bool controller_mqtt_online(void){return false;}

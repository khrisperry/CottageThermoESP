#pragma once
#include "link_protocol.h"
#include "esp_err.h"
typedef void (*tl_radio_handler_t)(const tl_packet_t *request,tl_packet_t *reply);
/* Configured encrypted unicast only. Must start after Wi-Fi. */
void tl_radio_start(tl_radio_handler_t handler);
bool tl_radio_configured(void);
bool tl_radio_online(void);
esp_err_t tl_radio_exchange(const tl_packet_t *request,tl_packet_t *reply);
/* Wi-Fi owner calls only while disconnected and NOT connecting/scanning.
 * Stops channel hopping while a recent authenticated exchange is healthy. */
void tl_radio_search_tick(void);

void tl_radio_start_config(tl_radio_handler_t handler,bool enabled,const char *peer,const char *key);

#pragma once
#include "host.h"
void *esp_netif_get_handle_from_ifkey(const char *key);
bool esp_netif_is_netif_up(void *netif);

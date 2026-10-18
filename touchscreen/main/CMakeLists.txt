idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_http_server esp_http_client
    esp_driver_i2c esp_timer nvs_flash espressif__cjson
    EMBED_TXTFILES "room.html")

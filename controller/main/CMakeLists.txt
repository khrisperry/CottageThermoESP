idf_component_register(SRCS "main.c" "controller_mqtt.c" "controller_integrations.c" INCLUDE_DIRS "."
    PRIV_REQUIRES mbedtls thermo_link esp_wifi esp_event esp_netif esp_http_server esp_driver_gpio
    esp_driver_ledc nvs_flash esp_timer lwip espressif__cjson thermo
    EMBED_TXTFILES "ui.html")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../secrets.h")
idf_component_register(SRCS "main.c" "controller_mqtt.c" INCLUDE_DIRS "."
    PRIV_REQUIRES espressif__mqtt mbedtls thermo_link esp_wifi esp_event esp_netif esp_http_server esp_driver_gpio
    esp_driver_ledc nvs_flash esp_timer lwip espressif__cjson thermo
    EMBED_TXTFILES "ui.html")

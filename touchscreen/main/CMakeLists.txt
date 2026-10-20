set(HA_MAPPING "${CMAKE_CURRENT_LIST_DIR}/../../ha_bridge/entities.json")
set(HA_GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../../tools/generate-ha-entities.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HA_MAPPING}" "${HA_GENERATOR}" "${CMAKE_CURRENT_LIST_DIR}/../secrets.h")
execute_process(COMMAND "${PYTHON}" "${HA_GENERATOR}" "${HA_MAPPING}" "${CMAKE_CURRENT_BINARY_DIR}/room_ha_entities.h" COMMAND_ERROR_IS_FATAL ANY)
idf_component_register(SRCS "main.c" "ha_panel.c" "room_transport.c" "room_display.c" "room_ha.c" "wifi_setup.c" INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_http_server esp_http_client
    esp_driver_i2c esp_timer nvs_flash lwip mbedtls espressif__cjson waveshare__esp32_s3_touch_lcd_4b lvgl__lvgl espressif__esp_jpeg
    EMBED_TXTFILES "room.html" "wifi_setup.html" "../../ha_bridge/home.html")

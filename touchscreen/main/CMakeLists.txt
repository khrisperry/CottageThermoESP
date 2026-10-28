set(HA_MAPPING "${CMAKE_CURRENT_LIST_DIR}/../../ha_bridge/entities.json")
set(HA_GENERATOR "${CMAKE_CURRENT_LIST_DIR}/../../tools/generate-ha-entities.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${HA_MAPPING}" "${HA_GENERATOR}" "${CMAKE_CURRENT_LIST_DIR}/../secrets.h")
execute_process(COMMAND "${PYTHON}" "${HA_GENERATOR}" "${HA_MAPPING}" "${CMAKE_CURRENT_BINARY_DIR}/room_ha_entities.h" COMMAND_ERROR_IS_FATAL ANY)
idf_component_register(SRCS "main.c" "room_transport.c" "room_discovery_probe.c" "room_display.c" "room_ha.c" "wifi_setup.c" "room_settings.c" "room_audio.c" "room_bmp280.c" "room_controller.c" "room_ships.c" "room_ships_decode.c" "room_ota.c" "room_mqtt.c" INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}"
    PRIV_REQUIRES mdns espressif__mqtt app_update esp_app_format thermo_link esp_wifi esp_event esp_netif esp_http_server esp_http_client
    espressif__esp_codec_dev esp_system esp_driver_i2c esp_timer nvs_flash lwip mbedtls espressif__cjson waveshare__esp32_s3_touch_lcd_4b lvgl__lvgl espressif__esp_jpeg
    EMBED_TXTFILES "wifi_setup.html" "settings.html")


# Keep display callbacks below the old overflow-sized frames; emit audit data.
set_source_files_properties(room_display.c PROPERTIES COMPILE_OPTIONS "-fstack-usage;-Wstack-usage=3072")

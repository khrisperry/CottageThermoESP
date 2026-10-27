idf_component_register(SRCS "main.c" "controller_setup.c" "controller_encoder.c" "controller_pwm_duty.c" "controller_mqtt.c" "controller_integrations.c" INCLUDE_DIRS "."
    PRIV_REQUIRES mdns mbedtls thermo_link esp_wifi esp_event esp_netif esp_http_server esp_driver_gpio
    esp_driver_i2c esp_driver_ledc nvs_flash esp_timer lwip espressif__cjson thermo
    EMBED_TXTFILES "ui.html")

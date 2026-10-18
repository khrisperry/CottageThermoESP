idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        esp_driver_gpio
        esp_driver_ledc
        nvs_flash
)

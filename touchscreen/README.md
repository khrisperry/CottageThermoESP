# Waveshare 4B room thermostat — V1.0.X

Both applications use the repository `VERSION` file. See the [production build and upgrade guide](../docs/PRODUCTION.md) and [V1.0.0 release notes](../docs/releases/V1.0.0.md).

Hardware: ESP32-S3 Touch-LCD-4B, 480×480, 16 MB flash, octal PSRAM. Fresh production builds select the SHT45/SHT4x primary probe on SDA47/SCL48. Existing local sensor configuration is retained unless `-Sht45` is explicitly passed to the 4B helper.

The display reports primary temperature to the controller over LAN with paired encrypted ESP-NOW fallback. Climate, Home, Laundry, Camera, Ships, clock screensaver, temperature offset, direct HA, MQTT Climate, furnace power monitoring and outdoor weather are included. HA services recover independently of the controller link. Thermal learning and automatic overshoot trim remain disabled in this baseline.

Settings and supported application OTA updates use the device webpage. The application identity, OTA layout, credentials and NVS schema are unchanged. The initial rollback-capable partition layout/bootloader must already have been installed over USB before application-only OTA is used.

[Historical touchscreen development notes](../docs/history/touchscreen-development.md).

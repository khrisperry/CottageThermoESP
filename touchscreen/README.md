# Future room touchscreen (not yet implemented)

Planned: Waveshare **ESP32-S3-Touch-LCD-4B** or equivalent 4-inch display. Native ESP-IDF + LVGL, remote temperature sensor (SHT31/SHT4x class; placement away from LCD heat), local touchscreen for room temperature, desired temperature, mode, schedule and predicted warm-up.

Direct authenticated Wi-Fi messaging to furnace controller; works independent of Home Assistant. ESP controller remains source of truth for schedules/setpoints and servo position. Room probe is primary thermostat input. Display should be able to reboot independently, with the controller retaining the last verified mechanical setting when temperature reports become stale.

**Not yet built**. Firmware will begin after furnace-side bench prototype and protocol definitions.

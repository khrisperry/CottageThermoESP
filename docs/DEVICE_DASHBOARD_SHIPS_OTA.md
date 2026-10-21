# Device dashboard, ships and OTA — 2026-10-21

Review branch: `feature/connectivity-climate-review`. Panel **v0.11.0**, controller **v0.4.0**. This extends the earlier connectivity review; Waveshare ESP32-S3-Touch-LCD-4B is the production display plan. No board was flashed by the coding environment.

## Install this update once with USB

From the ESP-IDF 6.0.1 PowerShell terminal:

```powershell
git switch feature/connectivity-climate-review
git pull --ff-only
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

Omit `-Bme280Test` to retain your configured permanent sensor. The BME280 option remains a **local temperature test**, independent of controller/HA availability, and does not report control samples to the furnace. Existing Wi-Fi, HA credentials, controller address and sound volume remain in NVS. Do not erase flash.

This initial USB flash installs a rollback-capable bootloader and the expanded partition table. The old firmware cannot bootstrap OTA by uploading only the new application. The helper preserves the existing NVS and factory addresses, enables rollback and selects a 256 KiB LVGL arena allocated in PSRAM.

For controller MQTT/ESP-NOW configuration from the webpage, also update the controller:

```powershell
.\tools\build-local.ps1 -Project controller
cd controller
idf.py -p YOUR_CONTROLLER_PORT flash monitor
```

Use the actual AtomS3 Lite port, not the display's COM14. The controller helper expands its former 1 MiB app slot to 2 MiB, retaining NVS at its existing address. Controller updates remain USB-based; the OTA uploader described here updates the wall display only.

## Display and webpage

Tap the top status pill for the aligned **Service / Status** table. Green means connected/fresh, yellow means not configured or radio standby, red means offline/stale, and orange means active ESP-NOW fallback. The gear remains an icon only. The global status prioritizes confirmed fallback and heat/HA outages; the table exposes individual services, including local sensor, ship tracking and controller MQTT.

On your normal Wi-Fi, open `http://DEVICE_IP/settings`. The IP and a persistent 12-digit hexadecimal **web access code** appear under the display's gear. Live status can be read without unlocking; configuration and firmware uploads require that code. It stays in browser memory for this tab, is not stored in browser storage, and is not included in status/config responses. Local HTTP is intended for the trusted device LAN.

The desktop webpage has a sidebar, matching colored connection rows, aligned form fields and a responsive phone layout. Its sections are:

| Section | Configuration |
| --- | --- |
| Wi-Fi | Test and save a new SSID/password; failed tests retain the previous profile |
| Home Assistant | HTTPS origin and long-lived REST token; blank retains the token, Clear removes it |
| Controller | IP/name/origin and its separate 32-character pairing token |
| MQTT climate | Broker host, port, TLS, username/password, discovery prefix and device name |
| ESP-NOW | Enable paired fallback, controller station MAC and private 32-hex shared key |
| Ship tracking | Existing service origin, enable/disable and 10/30/50/75/100-mile range |
| Sound & display | Volume, sound test and camera Fit/Fill (camera choice applies for this boot) |
| System | Firmware version/slot/board and application `.bin` uploader |

Profile changes save then restart the display. Sound/volume applies immediately; volume persists. Saved secrets are never returned; Show reveals only text entered in this tab. Blank secrets retain existing values; MQTT has an explicit Clear password option. The original `/wifi` captive setup remains available automatically if Wi-Fi fails; no Open Network Setup button is reintroduced.

MQTT settings configure the **paired controller's** publisher, preserving its stable MAC-based HA climate identity. No broker runs on the panel and no second climate entity is created. The HA REST token is separate from MQTT credentials. Pending controller configuration is retried in the background over paired Wi-Fi HTTP, not over ESP-NOW. The Overview shows Pending controller / Applied. An identical persisted profile is safe to retry. Actual changes restart the controller in Hold with PWM/bench arming released; they never arm or synchronize an actuator. Flash v0.4.0 on the controller before using this flow. Its old secrets/menuconfig defaults still work until explicitly managed from the panel.

## Upload future display updates

1. Build with `tools/build-room-4b.ps1` (no `-Flash` needed).
2. Open the device's `/settings`, unlock, then choose **System**.
3. Select **`touchscreen/build-4b/cottage_thermo_room_4b.bin`**.
4. Click **Upload firmware**. The page shows transfer progress, validation/restart status and the version after a new boot is observed. Keep power connected until it returns.

Use only the application file for this 4B build. Bootloader, partition-table, controller and merged flash images are rejected. The endpoint requires the local access code and matching browser origin, checks size, ESP32-S3 image identity and the exact `cottage_thermo_room_4b` project, then streams into an inactive 4 MiB slot. Only a fully validated image is selected for boot. Interrupted/rejected uploads retain the previous selection. A pending image is confirmed after display/touch/network/task initialization; bootloader rollback remains available if initialization fails before confirmation. This is image-integrity/board checking, not signed-firmware authentication or a guarantee about later application behavior.

Partition layout on the 16 MiB panel:

| Partition | Offset | Size |
| --- | --- | --- |
| NVS | `0x9000` | `0x6000` |
| PHY | `0xf000` | `0x1000` |
| Factory | `0x10000` | `0x400000` |
| OTA 0 | `0x410000` | `0x400000` |
| OTA 1 | `0x810000` | `0x400000` |
| OTA data | `0xc10000` | `0x2000` |

## Current ship tracker integration

The new **Ships** page reads the existing Tawas ship tracker **service 1.4.3**, reviewed at commit `94f9907`. Default origin: `https://ships.freshwater-tawas.us`, range 30 miles. Service code/deployment and AIS ingestion credentials are unchanged.

The panel uses `/api/v1/snapshot?range_miles=N&limit=12` and the existing 456×400 RGB565 `/api/v1/map.rgb565?range_miles=N&encoding=raw` endpoint. It overlays bounded numbered vessel markers and a cottage marker, with Previous/Next selection, distance, speed, position age, destination and OSM attribution. A healthy empty range says no ships; cached positions with an offline AIS feed are identified separately. Positions may already be aged at the service; the display shows that age rather than implying real-time navigation.

AIS processing and map generation remain on CottageServices. The display runs ship networking in a low-priority worker, polls every 10 seconds while Ships is visible and every 60 seconds elsewhere, and downloads/caches one map. Snapshot bodies are capped at 24 KiB, 12 vessels and 16 JSON nesting levels. The map is exactly 364,840 bytes including its validated 40-byte header. Most data allocations use PSRAM. Snapshot receive timeout is 3 seconds with a 4-second data deadline. Map requests allow 20 seconds with a 25-second data deadline, because the live service's cold map request took about 12 seconds in this environment. Map failures retry after 60 seconds; they do not block the UI. Ship and camera transfers pause during OTA. Ship networking pauses during ESP-NOW fallback.

## Validation and on-device review

Software checks include both ESP-IDF 6.0.1/GCC 15.2 firmware builds with ESP-NOW compiled in and controller MQTT configured using dummy credentials; no hardware was flashed. Two shared engine/protocol tests, seventeen native host tests, and the actual webpage's Chromium desktop/mobile test pass. The isolated Linux environment needs a component-manager parent-PID workaround because its `/proc` omits transient children; this is not a repository change or Windows requirement.

The native LVGL test uses the same bounded 256 KiB arena as firmware. It covers aligned status rows, gear, five-page navigation, ship selection, empty data and 100 map replacements without retained LVGL allocations. The actual 1.4.3 snapshot decoder passed 2,000 refreshes with zero retained allocations and a peak of 54,322 bytes in the contract fixture. A live 100-mile snapshot contained 14 nearby / 12 returned vessels and also passed decoding/stress checks. A live 30-mile map had the expected header, bounds and exact size. Test screenshots use simulated ships/map data; they are not measurements from the ESP.

Backend tests exercise old profile migration, secret redaction/retention, local access/origin checks, controller config persistence/retries, and inactive-slot OTA failure/success paths. The generated ESP application binary also passes the uploader's real header check. Browser tests cover unlocking, MQTT/ship/sound settings, secret controls, phone overflow, upload progress and version/reboot detection, with no page errors.

The webpage Overview reports actual on-device last/peak LVGL frame duration, free/largest internal memory, free PSRAM, ship payload/latency and controller sync status. These enable the physical performance review; host tests cannot establish ESP frame rate, radio behavior or power-loss recovery.

Next hardware review: navigate between Ships/Camera/Home, operate consecutive HA controls, inspect frame/memory metrics during repeated visits, and test a second application upload through each OTA slot. Then test interrupted upload, restart/rollback behavior and live controller MQTT/radio pairing. Preserve the earlier mechanical and permanent sensor checks in `CONNECTIVITY_REVIEW.md`.

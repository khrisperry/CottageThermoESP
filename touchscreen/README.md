# Cottage room panel — Waveshare ESP32-S3-Touch-LCD-4B

Version 0.5.0 provides the **native 480×480 LCD and capacitive touch interface**. This is for the **4B**, with 16 MB flash and 8 MB octal PSRAM. It uses Waveshare BSP 2.0.0, LVGL 9.2.0 and ESP LVGL port 2.6.2, compiled with ESP-IDF 6.0.1. The dependency lock pins the tested graph. **The ESP connects directly to Home Assistant using your long-lived token; no bridge process, bridge address or bridge token is required.** No device has been flashed or physically verified by the coding environment.

## Native pages

- **Climate** is the default. Shows the controller's reported temperature, effective target, operating mode and reason. Adjust in 0.5°F steps with −/+, then tap **Set target**. That writes only `target_f` (45–80°F); it cannot arm the servo, switch operating mode or start calibration.
- **Home** provides explicit Deck/Breezeway On/Off, blinds Open/Stop/Close, Kids Heat On/Off with its separate temperature, and vacuum Start/Stop. Unsupported or unavailable requests are disabled.
- **Laundry** shows the four status sensor strings, without invented progress or completion estimates.
- **Camera** requests 480×320 JPEG snapshots directly from Home Assistant only while visible, up to one per second. Old images hide on failure, stale data or navigation. An already-running read may finish within its timeout, but its result is discarded after a page change; no further frames are requested. Full-motion video remains separate work.

Swipe left/right or tap the bottom navigation. After 60 seconds without navigation or a control tap, it returns to Climate and stops camera requests. Network work runs in separate tasks; the display task performs no network I/O. A background task reads the eleven configured entities, waits 5 seconds, then repeats. Individual HA reads are bounded to 32 KB. Missing entities disable only their own controls; a detected connection/authentication failure invalidates all HA controls immediately. Cached entities expire after 30 seconds. The native screen also disables controls if it has not refreshed its local model for 10 seconds. One pending native user request is allowed, expires after two seconds if the worker has not begun it, and is never automatically replayed. The last 32 HA request IDs are held for this boot so a duplicate does not repeat an ambiguous service call. An uncertain command result asks you to inspect state before intentionally trying again.

The browser interfaces remain at `/` (Climate) and `/home` (HA). On the ESP both require the locally configured room pairing token for API access. The native HA panel needs only the HA token from the local secrets file, without a pairing screen. The existing controller pairing token still protects furnace communication and optional browser API access; it is separate from HA authentication.

## Windows build and first hardware test

1. Pull the `feature/thermostat-foundation` branch. From repository root, create your local credential file **once**:

   ```powershell
   Copy-Item .\touchscreen\secrets.h.example .\touchscreen\secrets.h
   ```

   Create/copy your long-lived token from Home Assistant profile → Security → Long-lived access tokens. Edit `touchscreen/secrets.h` locally:

   ```c
   #pragma once
   #define COTTAGE_HA_URL "https://cottageha.greenledges.net:8123"
   #define COTTAGE_HA_TOKEN "PASTE_YOUR_LONG_LIVED_TOKEN_HERE"
   ```

   The URL must be an HTTPS origin with **no trailing slash**. The real `secrets.h` and its backup variants are ignored by git; only the blank example is committed. The header is compiled into the local firmware image, so keep that image private. The application does not print or return the HA token. If the file is missing, the build helper creates a blank copy automatically. A blank token builds normally and shows a setup message without making HA requests. Do not overwrite an existing secrets file when pulling later updates.

2. From a **VSCode ESP-IDF 6.0.1 terminal**:

   ```powershell
   .\tools\build-room-4b.ps1 -Configure
   ```

   This uses ignored **touchscreen/sdkconfig.4b** and **touchscreen/build-4b/**, separate from the old browser-only configuration. Under **Cottage room thermostat**, configure:

   | Setting | Value |
   |---|---|
   | Wi-Fi SSID / password | Wi-Fi that can reach your HA HTTPS URL |
   | Controller URL / pairing token | Existing furnace controller settings, if present; not required for native HA-only testing |
   | Native Waveshare 4B display | Enabled |
   | Rotate display and touch 180 degrees | Enable if required by your mounting |
   | External probe | Keep disabled until wired and confirmed |

   The old bridge URL/token options have been removed. There is no HA token field in menuconfig: the token comes from `secrets.h`. Existing Wi-Fi and controller settings in `sdkconfig.4b` are retained. HTTPS uses the certificate bundle and waits for NTP time before connecting. Redirects are refused. A 401/403 response pauses HA requests until reboot; update the secret and rebuild/reflash if it was rejected.

3. Connect the **4B display**, identify its COM port, then explicitly flash:

   ```powershell
   .\tools\build-room-4b.ps1 -Flash -Monitor -Port COMxx
   ```

   Replace `COMxx` with its actual port. The helper builds without flashing unless `-Flash` is specified. Serial monitor exits with Ctrl+]. The display DHCP hostname is `CottageThermostat`. Changes to `secrets.h` require a rebuild and flash.

4. You can stop the old Windows bridge; the ESP does not use it. Confirm screen orientation, touch alignment and all four pages. Test Deck On/Off first, then the remaining HA controls. Open/leave Camera and confirm the old picture disappears. With no furnace controller connected, Climate shows unavailable values while native HA pages remain usable. Check live camera colors, touch responsiveness and absence of RGB artifacts on the physical unit.

Camera requests use `/api/camera_proxy/camera.cottagecamptz_fluent?width=480&height=320`, with bearer authentication in the header. Home Assistant does the requested image scaling; no separate host service is involved. Camera responses are bounded to 256 KB; the native decoder rejects images larger than 480×320 or unsupported JPEGs and clears the old picture. Full-motion HLS/WebRTC remains future work. Camera attributes and transient camera tokens are not exposed to the panel.

## Probe and controller separation

The native 4B uses the board I2C bus on **SDA GPIO47 / SCL GPIO48** for touch and the IO expander. An optional SHT31/SHT4x probe must share that bus at address 0x44 using 3.3V-compatible wiring; select its type and these pins in menuconfig only after confirming wiring. Other GPIO selections are rejected for the probe in native mode, preventing accidental reuse of LCD pins. The BSP owns I2C initialization. No secondary bus is initialized over display pins.

Valid CRC-checked room samples go directly to the independent controller every 15 seconds and are never queued for later replay. Kids temperature remains display-only and is never reported as a furnace temperature. The controller retains its existing simulation/bench protections and settings. **Servo movement and mechanics remain unverified.**

The browser-only fallback is available by disabling the native display setting; its generic probe pins must then be configured explicitly. Native builds require the 16 MB/PSRAM hardware profile in the new defaults; do not flash an old 8 MB browser build expecting a native LCD.

## Validation

- Native room image compiles under ESP-IDF 6.0.1; 4 MB application partition leaves substantial space.
- `tests/native` uses real LVGL on a host with mocked network/RTOS. It renders the same C UI and checks swipe navigation, control routing, target-only changes, stale/expired requests, unknown outcomes, the probe pin guard and return to Climate. It does not validate the physical RGB/touch electronics.
- Direct HA host checks exercise the production C client with a fake transport: exact eleven entities, bearer header, service paths, filtered attributes, missing/stale data, unsupported features, request deduplication, unknown outcomes, camera size request and authentication backoff. A separate empty-token test proves no HA requests are sent without configuration. No real cottage HA token is available to the coding environment; live direct-ESP validation awaits your local setup.
- Existing browser checks continue. The earlier PC bridge remains an optional test helper, but is not used by the ESP.

Hardware/API sources: [Waveshare 4B examples](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4B/ESP-IDF/), [Waveshare BSP](https://components.espressif.com/components/waveshare/esp32_s3_touch_lcd_4b/versions/2.0.0), [LVGL 9.2](https://docs.lvgl.io/9.2/).

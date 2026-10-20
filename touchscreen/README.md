# Cottage room panel — Waveshare ESP32-S3-Touch-LCD-4B

Version 0.7.0 provides the **native 480×480 LCD and capacitive touch interface**. This is for the **4B**, with 16 MB flash and 8 MB octal PSRAM. It uses Waveshare BSP 2.0.0, LVGL 9.2.0 and ESP LVGL port 2.6.2, compiled with ESP-IDF 6.0.1. The dependency lock pins the tested graph. **The ESP connects directly to Home Assistant using your long-lived token; no bridge process, bridge address or bridge token is required.** No device has been flashed or physically verified by the coding environment.

## Native pages

v0.7.0 replaces the initial flat layouts with a dark navy theme, mint/amber accents, an actual temperature dial, state-colored Home cards, four laundry tiles, a framed camera view and persistent navigation. Unavailable controller data remains visibly unavailable; the UI does not invent sensor values or cycle progress. The earlier visual references could not be opened in the coding session, so this revision is a native design pass rather than a verified reproduction of those images.


- **Climate** is the default. Shows the controller's reported temperature, effective target, operating mode and reason. Adjust in 0.5°F steps with −/+, then tap **Set target**. That writes only `target_f` (45–80°F); it cannot arm the servo, switch operating mode or start calibration.
- **Home** provides explicit Deck/Breezeway On/Off, blinds Open/Stop/Close, Kids Heat On/Off with its separate temperature, and vacuum Start/Stop. Unsupported or unavailable requests are disabled.
- **Laundry** shows the four status sensor strings, without invented progress or completion estimates.
- **Camera** requests 480×320 JPEG snapshots directly from Home Assistant only while visible, at most one per second after the previous request completes. Old images hide on failure, stale data or navigation. An already-running read may finish within its timeout, but its result is discarded after a page change; no further frames are requested. Full-motion video remains separate work.

Swipe left/right or tap the bottom navigation. After 60 seconds without navigation or a control tap, it returns to Climate and stops camera requests. Network work runs in separate tasks; the display task performs no network I/O. A background task reads the eleven configured entities, waits 5 seconds, then repeats. Polls, button commands and camera reads share a single serialized persistent HTTPS client. Commands take priority between entity polls. Successful responses keep the connection open when HA permits it; failed connections are discarded for the next request, and service calls are never automatically replayed. TLS allocations use PSRAM to preserve internal memory for the RGB display; the Windows helper migrates existing configs. Individual HA reads are bounded to 32 KB. Missing entities disable only their own controls; a temporary request failure retains recent state until its normal 30-second expiry; an authentication failure invalidates all HA controls immediately. Cached entities expire after 30 seconds. The native screen also disables controls if it has not refreshed its local model for 10 seconds. Tap **Wi-Fi** in the header to open the captive setup network. One pending native user request is allowed, expires after two seconds if the worker has not begun it, and is never automatically replayed. The last 32 HA request IDs are held for this boot so a duplicate does not repeat an ambiguous service call. An uncertain command result asks you to inspect state before intentionally trying again.

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

2. From a **VSCode ESP-IDF 6.0.1 terminal**, build and flash the display using its actual COM port:

   ```powershell
   .\tools\build-room-4b.ps1 -Flash -Monitor -Port COMxx
   ```

   Replace `COMxx` with the display port. There is no need to enter Wi-Fi in menuconfig. The helper preserves the local HA secrets and existing board/controller settings. It also raises older main-task stack settings to at least 16384 bytes, addressing the display-startup overflow observed on the physical board. Fresh builds have the same stack default. A manually increased larger stack is preserved.

3. With no saved network, the display opens **CottageThermo-XXXX-Setup** immediately. If the legacy/current saved network fails, setup opens after about **30 seconds** without an IP address. Join this setup network from your phone or PC. Stay connected if it warns that the network has no Internet.

   The captive page should open; if it does not, browse to **http://192.168.4.1**. Use **Scan for networks**, select the 2.4 GHz SSID (or enter a hidden SSID manually), enter its password, and press **Save and connect**. Open networks require the explicit no-password option. The page supports WPA-Personal passphrases (8–63 printable characters) and 64-digit hex PSKs.

4. The setup AP stays available while the ESP tests the network. New credentials are committed to the `room_wifi` NVS namespace **only after the ESP gets an IP address**. Wrong credentials time out after about 30 seconds and leave the previous saved profile intact. The status page shows the connection result/IP and numeric disconnect reason, never a stored password. The application logs disconnect reasons without logging credentials. No HA token is entered on this page; it still comes from `secrets.h`.

5. After a new connection is saved, the setup AP closes after **30 seconds**. Rejoin your normal network. On reboot, the ESP reconnects using NVS. Existing menuconfig credentials are attempted once for migration if there is no saved profile; the old default bench SSID is excluded. Legacy fields remain optional and are no longer the normal setup path.

6. To change networks later, tap the screen's **Wi-Fi** button. It opens the same captive portal while keeping the station connected where possible. If opened manually while already online and no new network is saved, setup closes after five minutes. A prolonged loss of the configured network also reopens setup automatically. Reconnect attempts use a short backoff rather than immediate retries inside the Wi-Fi event callback.

The setup page and credential submission are available only through the setup AP interface. Ordinary LAN requests cannot change Wi-Fi settings. The root path and common HTTP captive-detection requests redirect/show setup on the AP; the normal thermostat browser remains at the station IP. DHCP advertises the local DNS server and portal URI; DNS maps A queries to 192.168.4.1. Automatic popup behavior varies by phone/OS, so the direct address remains supported.

Check the screen orientation, Wi-Fi button, all four pages, successful NVS reconnect after reboot, and fallback after a bad password. Verify the display can reach HA directly. With no furnace controller connected, Climate remains unavailable while HA pages can work. Keep the external probe disabled until wiring is confirmed. To change orientation or controller/probe settings, use `-Configure` as before.

Camera requests use `/api/camera_proxy/camera.cottagecamptz_fluent?width=480&height=320`, with bearer authentication in the header. Home Assistant does the requested image scaling; no separate host service is involved. Camera requests allow 15 seconds for HA to obtain a still. Camera responses are bounded to 768 KB; the native decoder now accepts larger baseline JPEGs and chooses 1/2, 1/4 or 1/8 decode scaling to keep the decoded output at or below 480×320. Progressive JPEGs, invalid data, images too large even at 1/8 scale and unsupported formats produce specific on-screen errors. Full-motion HLS/WebRTC remains future work. Camera attributes and transient camera tokens are not exposed to the panel.

## Probe and controller separation

The native 4B uses the board I2C bus on **SDA GPIO47 / SCL GPIO48** for touch and the IO expander. An optional SHT31/SHT4x probe must share that bus at address 0x44 using 3.3V-compatible wiring; select its type and these pins in menuconfig only after confirming wiring. Other GPIO selections are rejected for the probe in native mode, preventing accidental reuse of LCD pins. The BSP owns I2C initialization. No secondary bus is initialized over display pins.

Valid CRC-checked room samples go directly to the independent controller every 15 seconds and are never queued for later replay. Kids temperature remains display-only and is never reported as a furnace temperature. The controller retains its existing simulation/bench protections and settings. **Servo movement and mechanics remain unverified.**

The browser-only fallback is available by disabling the native display setting; its generic probe pins must then be configured explicitly. Native builds require the 16 MB/PSRAM hardware profile in the new defaults; do not flash an old 8 MB browser build expecting a native LCD.

## Validation

- v0.7.0 source reviewed against LVGL 9.2 and the Espressif JPEG decoder. The earlier generic camera failure did not identify whether HTTP, image dimensions or decoding failed. This revision adds stage-specific on-screen messages and rate-limited logs with HTTP status, byte count and source dimensions. Physical camera playback remains unverified. The code workspace is unavailable, so firmware compilation, C tests and rendered native previews could not run for this revision.


- v0.5.0 compiled under ESP-IDF 6.0.1 and the user confirmed its local build. The user confirmed the v0.6.1 captive portal and direct HA controls work on the board. v0.6.2 addresses TLS allocation failures observed when polling overlaps button commands. v0.6.3 fixes a request-setup regression that prevented the first HA GET from reaching the network, and adds HA startup/failure/connected logs. These source/API changes have been reviewed against IDF 6.0.1 but **have not been compiled or hardware tested in the unavailable coding workspace**. Run the local build above, then repeatedly toggle lights while polling and viewing the camera.
- `tests/native` uses real LVGL on a host with mocked network/RTOS. It renders the same C UI and checks swipe navigation, control routing, target-only changes, stale/expired requests, unknown outcomes, the probe pin guard and return to Climate. It does not validate the physical RGB/touch electronics.
- Direct HA host checks exercise the production C client with a fake transport: exact eleven entities, bearer header, service paths, filtered attributes, missing/stale data, unsupported features, request deduplication, unknown outcomes, camera size request and authentication backoff. A separate empty-token test proves no HA requests are sent without configuration. No real cottage HA token is available to the coding environment; the user has confirmed direct ESP connectivity; the v0.6.2 connection reuse changes still require local validation.
- Existing browser checks continue. The earlier PC bridge remains an optional test helper, but is not used by the ESP.

Hardware/API sources: [Waveshare 4B examples](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4B/ESP-IDF/), [Waveshare BSP](https://components.espressif.com/components/waveshare/esp32_s3_touch_lcd_4b/versions/2.0.0), [LVGL 9.2](https://docs.lvgl.io/9.2/).

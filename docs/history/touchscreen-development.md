# Historical development notes

These entries describe earlier builds. For V1.0.X use the [production guide](../PRODUCTION.md). Old version numbers, branch names and build instructions below are historical.

# Cottage room panel — Waveshare ESP32-S3-Touch-LCD-4B

## v0.15.0 — persistent room temperature calibration and link diagnostics

**Primary sensor temperature offset:** Waveshare web settings now include a **Temperature** page with a signed **−10.0°F to +10.0°F** adjustment in **0.1°F** increments. The page displays the raw SHT45/SHT31 sensor reading, proposed corrected reading, a slider and numeric entry, plus Reset (sets preview to zero until saved). If the sensor reads warmer than a trusted room thermometer, enter a **negative** offset. The offset is validated server-side, stored separately as signed tenths under NVS `room_config/temp_offset_t10`, and loaded on boot with a zero default. Changing it **does not restart** the display or overwrite the existing settings profile (credentials, MQTT, ESP-NOW, HA, controller URL, Ships and OTA).

Every 15-second successful sensor sample is corrected exactly once **before** delivery to the screen, Home Assistant MQTT Climate state and the authenticated controller temperature sample. The raw value is retained separately in `/api/device/status` as `raw_temperature_f`; its corrected peer remains `temperature_f`, with `temperature_offset_f` reflecting the saved adjustment. Web Overview shows raw and corrected readings side by side. The correction does **not** change the furnace target setpoint, dial-position mapping or controller firmware. Corrections outside the current controller sample protocol range (32–110°F) are rejected and logged rather than sent out of bounds; this conservative pre-existing protocol range should be revisited before using the system for freeze protection in below-freezing conditions. Recheck the offset after the final enclosure is installed and sensor temperature stabilizes; screen self-heating and room airflow can bias an exposed bench sensor.

**LAN / ESP-NOW diagnostics:** When mDNS returns a controller IP but the Waveshare cannot establish HTTP to it, the link will **remain on ESP-NOW** until an authenticated LAN response succeeds. A browser on a different device being able to open the controller does not prove the Waveshare can open a TCP socket to it. The Controller page now reports selected transport, local Wi-Fi RF channel, recent ESP-NOW authenticated reply state, consecutive ESP-NOW status failures and LAN discovery HTTP error/status values. Use **Test Wi-Fi and ESP-NOW** to check both paths without moving the servo. The radio protocol, link selection, fallback implementation, MQTT, controller and servo controls are unchanged. Investigate AP wireless client isolation, VLAN forwarding and different Wi-Fi channels before attempting controller firmware changes.

**Only update Waveshare:** from the local ESP-IDF PowerShell, run:

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14
```

No GitHub Actions build, NVS erase, pairing change or Super Mini controller update. Run local host test `test_room_temp_cal` (registered in `tests/CMakeLists.txt`) and test serial log, web `Temperature` preview and saved value, Home Assistant Climate reading, and Controller transport diagnostics. Keep unattended heating in **Service** until sensor correction and dependable controller link are verified on the installed hardware.

## v0.14.9 — photographed-display polish (visual-only)

Reviewed five real Waveshare 480×480 photos from the previous Design #3 build. Main fixes:

- **Navigation:** the five icons previously overlapped or sat beside their labels because the button helper centered the LVGL label and subsequent x/y positioning did not reset its alignment. Labels are now explicitly aligned to the top-left coordinate system and centered in dedicated icon rectangles above the captions. The Home, Climate, Laundry, Camera and Ships route mapping remains unchanged.
- **Laundry:** the four blank white rings looked like progress meters but had no meaningful progress source. Each card now shows a plain, legible status tag (IDLE, DONE, PAUSED, ALERT or OFFLINE) when not active. A rotating indicator is shown **only while the machine is actually active**. Existing HA status, Notify buttons and sound logic remain unchanged.
- **Camera:** the rectangular black area in the photographed image was confirmed by the user to originate from the camera itself. It is *not* a firmware rendering problem. Camera image presentation, scaling, clipping, request/decode and status behavior remain unchanged; the speculative frame-redraw change was reverted.

This is a visual-only release; the thermostat/controller communication stack, sensor and temperature readings, ESP-NOW fallback, MQTT/Home Assistant commands, blower/servo calibration, clock screensaver and all five pages are unchanged.

**Build only locally, no GitHub Actions:** from your configured ESP-IDF PowerShell, run `git pull --ff-only` followed by `.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14`. Preserve NVS/pairing and leave controller v0.7.1 untouched.

## v0.14.8 — full-screen digital clock screensaver

The native Waveshare 4B automatically displays a large **cyan seven-segment
digital clock** after **60 seconds** with no touchscreen activity. It is a full
480×480 overlay that hides the page and bottom navigation, shows **local
12-hour time with AM/PM and weekday/date**, and dims display brightness from
the normal 80% to 48%. The colon blinks each second.

- **Touch anywhere to dismiss:** the overlay consumes the initial finger
  press and waits for release; waking must not also activate a hidden
  thermostat, light, blind, or Home Assistant button. Brightness restores
  immediately and the **same page and existing modal** return.
- The idle timer now opens the clock rather than automatically returning
  every screen to Climate after 60 seconds.
- Time comes from the touchscreen's **existing pool.ntp.org SNTP client**,
  formatted in the Michigan `America/Detroit` time zone using
  `EST5EDT,M3.2.0/2,M11.1.0/2` (automatic spring/fall daylight-saving
  transitions). Wi-Fi/HA are **not needed to continue ticking after a
  successful sync**. If freshly booted without time sync, the clock displays
  dashes and `Waiting for network time`, never an incorrect 1970 timestamp.
- No network, command, controller, thermostat, servo, NVS, MQTT, radio, or
  camera data flow is changed. Clock widgets are constructed once and
  updated at most once a second.
- Pure-C host test `test_room_clock` verifies the 60-second inactivity
  threshold, unsynchronized time guard, 12-hour formatting and AM/PM.

**Local build only**; no GitHub Actions builds. Flash **only the Waveshare**,
retaining all local settings and NVS. The Super Mini controller stays at
v0.7.1.

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14
```

After a local flash, leave the screen untouched for one minute, check clock
and daylight-saving local time, then tap the clock above the temperature
buttons or Home light tiles and confirm that no underlying action fires. The
ESP-IDF firmware build and on-device rendering are still to be confirmed
locally; pure host tests do not certify the hardware.

## v0.14.7 — faithful Design #3 composition (local build only)

The **original six-panel reference graphic** (selected Design #3, bottom-row Home and Climate concepts) was reviewed side by side with the firmware. Versions 0.14.5–0.14.6 only applied a color treatment; they did not reproduce the asymmetric layout, vivid card illustrations, or icon-led tab navigation. This release corrects those differences.

- **Climate:** the *proposed target* is now the large temperature headline on the **left**, with actual **Current** temperature immediately underneath. A genuine, still draggable **blue/orange 248px split gauge** occupies the right, with radial markers and a small two-line `DRAG RING / TO ADJUST` hint inside. The original controller-facing command flow is unchanged: drag previews, and the four buttons remain `−1 | Set Temp | +1 | Away/Home` on the bottom row. A narrow status strip preserves Live/Service/Bench state and failure reason without filling the gauge. The button styling follows Design #3's blue Set Temp action.
- **Home:** the same five existing connected HA controls are arranged in a **2-column grid** of icon-left, text-right blue/teal/amber cards. Deck/Breezeway tiles retain safe tap-to-toggle; Blinds retains Open/Stop/Close; Vacuum Start/Stop; Kids Heat On/Off and temperature. **No Camera card** is placed on Home and the sixth tile remains free.
- **Navigation:** shared across all screens with a compact **Cottage** header, a small tappable online health pill, and five **icon + label** tabs ordered Home / Climate / Laundry / Camera / Ships. The underlying page IDs and callbacks have not changed. Ships stays visible and its page content is not redesigned.
- **Laundry/Camera:** keep exactly their previous data and actions, with the reference design's layered blue cards and smaller accent treatments.

**Visual-only production file changes**: `touchscreen/main/room_display.c`, `touchscreen/main/room_layout.h`, host-only geometry regression test, version metadata and docs. No `room_controller.c`, `room_mqtt.c`, `wifi_setup.c`, ESP-NOW, temperature mappings, NVS, thermostat motor/servo logic, Home Assistant commands, camera streaming or any other backend behavior was changed.

**Build and flash locally** from your ESP-IDF PowerShell (the recent Waveshare port was COM14):

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14
```

**No GitHub Actions build** was triggered. This branch has source-level bounds checks and LVGL 9.2 API verification, but the completed ESP-IDF compile and the actual 480x480 rendering still need to be verified on the user's Windows machine before this is treated as released hardware firmware. No NVS erase, calibration, controller update or pairing changes are needed.

## v0.14.5 — Design #3 native LVGL visual refresh (local build)

**Visual-only upgrade** of the existing Waveshare ESP32-S3 Touch LCD 4B interface. The new shared **midnight-blue design system** uses brighter electric-blue/teal accents, warm amber for the heat target, high-contrast typography, rounded raised cards, consistent status colors, a unified header and a highlighted bottom navigation tab. No new bitmap assets, fonts, animation engines, web dependencies or LVGL alpha-layer effects were added. Ships is **still present and functional in navigation**; only its common header and navigation adopt the style, and its page contents have not been redesigned.

- **Climate:** enlarged draggable setpoint ring and one-row `-1 | Set Temp | +1 | Away/Home` controls remain. The dial sits in a single rounded dark surface, the setpoint knob has a brighter outline, and live/bench/service status is grouped in a subtle card with consistent accent color. Dragging remains a **preview only** until Set Temp is pressed.
- **Home:** preserves exactly the five compact 2-column tiles and the vacant sixth slot. Light tiles retain touch-to-toggle, golden lit bulbs, and known-state/pending command gating. Blinds retains Open/Stop/Close, Vacuum retains Start/Stop with a new lightweight LVGL robot-vacuum icon, and Kids Heat retains On/Off plus temperature. Active tiles receive a restrained tinted background and colored outline.
- **Laundry:** the original four equipment cards and their Notify buttons remain, but gain modern blue icon plaques, thin dividers, clear WASHER/DRYER labels, unified borders/spacing, consistent status capitalization and subtle animated activity rings.
- **Camera:** Parking Lot gets a matching title strip, connection/status label and border treatment. **Camera requests, media decoding, image scale, viewport dimensions and clip policy are unchanged.** There is no camera tile on Home.
- **Ships:** left as-is. It remains visible in the five-tab navigation and continues to function as before.

**No communication or controller changes whatsoever:** ESP-NOW fallback, Wi-Fi recovery/discovery, MQTT/HA climate/away, all Home Assistant actions, notification toggles, thermostat control and mode flags, calibration, NVS, schedule and network timing remain unchanged. The only edited production code is the native Waveshare `room_display.c` styling, plus the displayed firmware version.

**Local flash only; GitHub Actions not used:** run in the Windows ESP-IDF 6.0.1 PowerShell from the project root:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21
```

Controller Super Mini **v0.7.1 stays unchanged**. Do not erase NVS. After flashing, verify the rounded card text doesn't overlap, all five tabs including Ships still switch, Climate drag/setpoint buttons behave exactly as before, lights and cover/vacuum/radiator controls work, Laundry notifications remain togglable, and Camera still displays its feed.

## v0.14.4 — one-row Climate controls and shorter Away display

On the Waveshare 480×480 Climate page, **all four buttons now share the same bottom row**: `-1 | Set Temp | +1 | Away` (or `Home` while Away is selected). They use the full 456px content width with eight-pixel gutters. The separate Away button row has been removed, giving the dial and status more room. The large target label reads only **Away** in Freeze Hold, while the existing controller-owned 50°F setting, Home resume behavior and Home Assistant climate presets remain unchanged. The ring hint is now two centered lines: **DRAG RING** / **TO ADJUST**; its knob and touch behavior are unchanged. The mode and status text span the full width below the dial.

The production geometry has compile-time assertions and a `test_room_home_layout` host regression covering all four bottom buttons. **Build only on the local ESP-IDF machine; no GitHub Actions run.** From the repository root after `git pull --ff-only`: `.\\tools\\build-room-4b.ps1 -Flash -Monitor -Port COM21`. Do not erase NVS or reflash controller v0.7.1.

## v0.14.3 — compact Home tiles and larger, unclipped Climate dial

The native Waveshare 480×480 Climate page now uses a larger 252px interactive dial, safely inset from the header so the knob is not clipped. **-1 / Set Temperature / +1** share a single wide bottom control row. **Away (50 F)** remains separate and the ring continues to change a draft only: no servo or network request is sent until **Set Temperature** is pressed.

The Home page is a compact **two-column grid** of 224px tiles:

- Row 1: **Deck Lights** and **Breezeway Lights**, each a whole-tile On/Off toggle with an illuminated gold bulb when On; unknown/stale states and in-flight commands cannot be tapped
- Row 2: **Blinds** with Open / Stop / Close icon buttons, and **Vacuum** with Start / Stop and dock status
- Row 3: **Kids Heat** with On / Off and current bedroom temperature, leaving the right-hand sixth tile position unused for future features

The tiles retain their existing Home Assistant entity mappings, queue, pending-command checks and state/availability behavior. No controller-side changes are required. The production layout constants and static bounds assertions live in `main/room_layout.h`, and `test_room_home_layout.c` checks grid geometry and known-state-only light toggling. **All flash/build work stays local** rather than using GitHub Actions. To update the 4B only:

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21
```

Leave the Super Mini controller on v0.7.1. This is a layout/UI release and does not require an NVS erase, recalibration, MQTT reconnection or new pairing.

## v0.14.2 — local-build Climate polish and reliable controller commands

The Climate page uses a **draggable target-temperature ring** with a visible handle (45–80°F, one-degree steps). The large center number remains the **current room temperature**, while the amber target indicator and ring show the *proposed setpoint*. Sliding the ring or tapping the small **−1 / +1** buttons only edits a draft; tap **Set Temperature** once to send it to the controller. **Away (50 F)** and **Resume Home** remain a prominent independent button. Raw firmware states such as `mapped_setting_reached` are translated to user-facing status labels, with consistent capitalization and no unsupported special-font separators.

Each deliberate target/Away request now obtains a **fresh read-only controller lease before the single mutation**, avoiding intermittently expired five-second leases. The previously ambiguous "Request rejected" toast now reports whether the controller was busy, denied a command (e.g. because Away is active), had an expired/replaced session, or had an uncertain reply. **Do not automatically retry an uncertain command**; inspect the reported setpoint first. Encrypted ESP-NOW fallback, controller authorization and calibrations are unchanged.

Home Assistant MQTT Climate now advertises **Home** and **Away** as explicit presets and reports Home when Away is off. Home Assistant *always* inserts its own built-in **None** entry; MQTT firmware cannot rename or suppress that built-in control. The existing separate **Away / Freeze Hold (50 F)** switch on the device is another convenient option. Republish discovery from the touchscreen settings after flashing.

**Build locally, no GitHub Actions required:** from your configured ESP-IDF 6.0.1 PowerShell in the repo root, after `git pull --ff-only`, run `.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21`. Preserve existing `sdkconfig.4b`, local secrets and NVS. **Controller v0.7.1 does not need reflashing.** Validate the slider touch and ±1 controls, one-time submit, Home/Away MQTT state, and reconnecting from Wi-Fi to ESP-NOW before the cottage installation.



Version 0.13.8 provides the **native 480×480 LCD and capacitive touch interface**. This is for the **4B**, with 16 MB flash and 8 MB octal PSRAM. It uses Waveshare BSP 2.0.0, LVGL 9.2.0 and ESP LVGL port 2.6.2, compiled with ESP-IDF 6.0.1. The dependency lock pins the tested graph. **The ESP connects directly to Home Assistant using your long-lived token; no bridge process, bridge address or bridge token is required.** No device has been flashed or physically verified by the coding environment.

The 4B is the planned production display. User tests confirmed the earlier UI, camera, audio and BME280 temperature test. v0.11.0 adds aligned colored Connections, full LAN configuration, Ships and application-bin OTA. v0.12.1 connects this display directly to MQTT for an HA Climate entity and monitoring sensors. HA target requests use the same queue as touchscreen changes; controller v0.4.1 removes controller MQTT commands. See [standalone MQTT monitoring](../STANDALONE_MQTT_MONITORING.md). Follow [dashboard, ships and OTA setup](../DEVICE_DASHBOARD_SHIPS_OTA.md), including the **initial USB flash required for OTA**. Earlier milestone notes below are historical.

For controller discovery, use the Controller settings page and select **Use automatic discovery**. The display resolves **CottageThermoController.local** through mDNS, verifies identity and shows the resolved IP. See [mDNS auto-discovery bench checks](../CONTROLLER_MDNS_AUTO_DISCOVERY.md).

## Display orientation

The display and touch default to **180 degrees**. The Windows build helper applies this to existing `sdkconfig.4b` files too, including OTA builds. Build normally to use the new orientation. Pass `-Rotation 0` to the helper to restore the original orientation; `-Rotation 180` selects the new default explicitly. Direct `idf.py` builds with an existing saved config require `CONFIG_ROOM_DISPLAY_ROTATE_180=y` (menuconfig → Cottage room thermostat).

## Device settings

Use `http://DEVICE_IP/settings` on normal Wi-Fi. Tap the display gear for its webpage address. The local webpage opens directly with configuration and OTA available; there is no access code. All HA/controller/MQTT/radio/ship fields are available there. Blank secrets retain existing values; explicit Clear controls remove supported secrets. Compile-time secrets/menuconfig remain boot defaults until a profile is saved.

With no saved Wi-Fi the captive setup network opens immediately; after a prolonged outage it opens automatically while reconnection continues. Join **CottageThermo-XXXX-Setup**, open `http://192.168.4.1`, and provision Wi-Fi first. The Wi-Fi portal handles networking; the full settings page is separate. There is no manual Wi-Fi button on the native screen.

## Native pages

v0.7.0 replaces the initial flat layouts with a dark navy theme, mint/amber accents, an actual temperature dial, state-colored Home cards, four laundry tiles, a framed camera view and persistent navigation. Unavailable controller data remains visibly unavailable; the UI does not invent sensor values or cycle progress. The earlier visual references could not be opened in the coding session, so this revision is a native design pass rather than a verified reproduction of those images.


- **Climate** is the default. Shows the controller's reported temperature, effective target, operating mode and reason. Adjust in 0.5°F steps with −/+, then tap **Set target**. That writes only `target_f` (45–80°F); it cannot arm the servo, switch operating mode or start calibration.
- **Home** provides explicit Deck/Breezeway On/Off, blinds Open/Stop/Close, Kids Heat On/Off with its separate temperature, and vacuum Start/Stop. Unsupported or unavailable requests are disabled.
- **Laundry** shows the four status sensor strings, without invented progress or completion estimates.
- **Ships** uses the existing Tawas service, a cached map and up to twelve selectable vessel markers. See the latest setup guide for bounds and performance instrumentation.
- **Camera** requests 480×320 JPEG snapshots directly from Home Assistant only while visible, at most one per second after the previous request completes. Old images hide on failure, stale data or navigation. An already-running read may finish within its timeout, but its result is discarded after a page change; no further frames are requested. Full-motion video remains separate work.

Swipe left/right or tap the bottom navigation. After 60 seconds without navigation or a control tap, it returns to Climate and stops camera requests. Network work runs in separate tasks; the display task performs no network I/O. A background task reads the eleven configured entities, waits 5 seconds, then repeats. Polls, button commands and camera reads share a single serialized persistent HTTPS client. Commands take priority between entity polls. Successful responses keep the connection open when HA permits it; failed connections are discarded for the next request, and service calls are never automatically replayed. TLS allocations use PSRAM to preserve internal memory for the RGB display; the Windows helper migrates existing configs. Individual HA reads are bounded to 32 KB. Missing entities disable only their own controls; a temporary request failure retains recent state until its normal 30-second expiry; an authentication failure invalidates all HA controls immediately. Cached entities expire after 30 seconds. The native screen also disables controls if it has not refreshed its local model for 10 seconds. The captive setup network opens automatically when Wi-Fi remains disconnected for 30 seconds. Native commands use a dedicated worker and an eight-entry FIFO. Only the device with a queued/in-flight command is disabled, so Deck then Breezeway taps are accepted independently. Requests expire after ten seconds if the command worker has not begun them and are never automatically replayed. A brief error toast overlays content; the persistent bottom Wi-Fi/status strip has been removed. The last 32 HA request IDs are held for this boot so a duplicate does not repeat an ambiguous service call. An uncertain command result asks you to inspect state before intentionally trying again.

The retired browser Climate/Home dashboards are no longer embedded or served. LAN requests to `/` and `/home` redirect to `/settings`; setup-network `/` still opens Wi-Fi provisioning. Native display controls and controller probe reporting continue independently.

## Windows build and first hardware test

1. Use `main` for the reviewed baseline or `feature/connectivity-climate-review` for the new connectivity work. From repository root, create your local credential file **once**:

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
   .\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14
   ```

   Replace `COM14` with the display port. There is no need to enter Wi-Fi in menuconfig. The helper preserves the local HA secrets and existing board/controller settings. It also raises older main-task stack settings to at least 16384 bytes, addressing the display-startup overflow observed on the physical board. Fresh builds have the same stack default. A manually increased larger stack is preserved.

3. With no saved network, the display opens **CottageThermo-XXXX-Setup** immediately. If the legacy/current saved network fails, setup opens after about **30 seconds** without an IP address. Join this setup network from your phone or PC. Stay connected if it warns that the network has no Internet.

   The captive page should open; if it does not, browse to **http://192.168.4.1**. Use **Scan for networks**, select the 2.4 GHz SSID (or enter a hidden SSID manually), enter its password, and press **Save and connect**. Open networks require the explicit no-password option. The page supports WPA-Personal passphrases (8–63 printable characters) and 64-digit hex PSKs.

4. The setup AP stays available while the ESP tests the network. New credentials are committed to the `room_wifi` NVS namespace **only after the ESP gets an IP address**. Wrong credentials time out after about 30 seconds and leave the previous saved profile intact. The status page shows the connection result/IP and numeric disconnect reason, never a stored password. The application logs disconnect reasons without logging credentials. HA credentials are configured separately through the `/settings` page or supplied as compile-time defaults in `secrets.h`.

5. After a new connection is saved, the setup AP closes after **30 seconds**. Rejoin your normal network. On reboot, the ESP reconnects using NVS. Existing menuconfig credentials are attempted once for migration if there is no saved profile; the old default bench SSID is excluded. Legacy fields remain optional and are no longer the normal setup path.

6. To change networks later, use the unlocked webpage's **Wi-Fi** section. A prolonged loss of the configured network also reopens the captive portal automatically. Reconnect attempts continue with backoff.

The setup page and credential submission are available only through the setup AP interface. The `/settings` webpage can also submit new credentials through its authenticated settings endpoint. The root path and common HTTP captive-detection requests redirect/show setup on the AP; the normal thermostat browser remains at the station IP. DHCP advertises the local DNS server and portal URI; DNS maps A queries to 192.168.4.1. Automatic popup behavior varies by phone/OS, so the direct address remains supported.

Check screen orientation, the status pill/diagnostics, gear Settings, all five pages, successful NVS reconnect after reboot, and provisioning after a bad password. Verify the display can reach HA directly. With no furnace controller connected, target controls remain unavailable while fresh local probe readings and HA pages can work. To change orientation or controller/probe settings, use `-Configure` as before.

Camera requests use `/api/camera_proxy/camera.cottagecamptz_fluent?width=480&height=320`, with bearer authentication in the header. Home Assistant does the requested image scaling; no separate host service is involved. Camera requests allow 15 seconds for HA to obtain a still. Camera responses are bounded to 768 KB; the native decoder now accepts larger baseline JPEGs and chooses 1/2, 1/4 or 1/8 decode scaling to keep the decoded output at or below 480×320. Progressive JPEGs, invalid data, images too large even at 1/8 scale and unsupported formats produce specific on-screen errors. Full-motion HLS/WebRTC remains future work. Camera attributes and transient camera tokens are not exposed to the panel.

## Probe and controller separation

The native 4B uses the board I2C bus on **SDA GPIO47 / SCL GPIO48** for touch and the IO expander. An optional SHT31/SHT4x probe (0x44) or BMP280/BME280 test probe (0x76/0x77) must share that bus using 3.3V-compatible wiring; select its type and these pins in menuconfig only after confirming wiring. Other GPIO selections are rejected for the probe in native mode, preventing accidental reuse of LCD pins. The BSP owns I2C initialization. No secondary bus is initialized over display pins.

Valid CRC-checked room samples go directly to the independent controller every 15 seconds and are never queued for later replay. Kids temperature remains display-only and is never reported as a furnace temperature. The controller retains its existing simulation/bench protections and settings. **Servo movement and mechanics remain unverified.**

The browser-only fallback is available by disabling the native display setting; its generic probe pins must then be configured explicitly. Native builds require the 16 MB/PSRAM hardware profile in the new defaults; do not flash an old 8 MB browser build expecting a native LCD.

## v0.8.0 controls and visuals

Lights have locally drawn bulb silhouettes: dark when off, yellow when on. Confirmed HA state drives the icon; a pending command shows Sending on that row. Kids heat, blinds and vacuum also change icon/color with state. Laundry classifies Washing/Drying/Rinsing/Spinning as active, Not in Use as idle, plus finished, paused and error states; colored tiles and rotating drum indicators distinguish running appliances without inventing a percentage. The reclaimed footer area enlarges all five pages.

## v0.8.1 feedback and completion sounds

Light/switch On/Off taps show the requested state immediately for up to five seconds. An entity sample newer than the tap confirms the displayed state; older cached samples cannot bounce the UI back during that window. If confirmation does not arrive, the UI returns to actual HA state. Explicit command rejection clears the prediction. Confirmed state still drives all controller behavior; this is only display feedback.

Each washer and dryer has an independent **Notify: Off / On** toggle. All four default off. Each opt-in applies to one cycle: when that appliance completes, its toggle returns to Off immediately and that reset is saved in the `room_alerts` NVS namespace. An enabled appliance plays a short, locally synthesized chime on a fresh active-to-idle/completed transition, then stops and mutes the output. Repeated idle readings, booting idle, missing entities and stale/offline gaps do not trigger completion sounds. Disabling a toggle before completion prevents the cue. On completion, permission is captured for that one cue before its toggle is automatically cleared; that cue can still play while the tile already shows Off. There is no persistent completion badge to dismiss. The 4B BSP/ES8311 speaker output is used; physical speaker playback is unverified.

## v0.8.2 audio correction

The first audio implementation used codec volume 30 (approximately -35 dB on the default codec curve) plus low-amplitude PCM. The correction uses 16 kHz stereo PCM duplicated to both slots, volume 75, a stronger 6000-peak tone, a 200 ms amplifier startup delay and a silence/drain interval before muting. Return codes for volume, mute and amplifier control are logged. Logs say PCM sent, not that a physical speaker was heard.

Tap **Settings → Test sound** to play an explicit test chime without changing any notification toggle or waiting for a laundry cycle. Completion notifications remain opt-in for one cycle per appliance.

## v0.8.3 volume and reset correction

The chime now uses codec volume **100** and full-scale 16-bit sine PCM (peak **32767**) on both stereo slots. This is the maximum software level at volume 100%. The Sound test and completion chimes use the same output settings.

Cycle history for the notification reset is now separate from freshness needed for a sound. A temporary HA failure, unavailable reading or stale gap suppresses an uncertain/stale chime but preserves the known running cycle. When a fresh idle/completed reading later arrives, that appliance's Notify toggle is cleared and saved even if the chime was suppressed. Logs report the reset and whether sound was suppressed.

## Validation

- v0.8.3 updates the production-audio fixture for volume 100/full-scale peak, adds cycle-history assertions across missing/stale samples, and checks the reset-only path through the direct HA fixture. Source reviewed; these revised C checks and full firmware could not run because the execution workspace is unavailable again. The v0.8.2 host-audio result below is historical, not validation of physical loudness or this revision.


- v0.8.2: production `room_audio.c` compiled with host GCC and mocked codec/RTOS/NVS under `-Wall -Wextra -Werror`. `audio_test` passed stereo/peak-level checks, startup/drain ordering, failure muting, explicit speaker testing, one-cycle auto-off and saved preference writes. ESP-IDF/toolchain and physical speaker remain unavailable, so the full firmware build and audible output still require local validation.


- v0.8.1 adds pure-C optimistic feedback/laundry transition tests and extends the native UI fixture for independent notification toggles and cached-state feedback. Codec/BSP calls were checked against vendor source. Compilation, rendered previews, NVS persistence and physical speaker testing remain unavailable in the coding workspace.


- v0.8.0 includes per-device pending/rapid-tap regression checks and a pure-C visual-state fixture. The settings page JavaScript was syntax-checked and exercised with mocked GET/POST responses, token clearing and access-denied messaging. C compilation, NVS/HTTP hardware behavior and native rendering remain unverified in the unavailable execution workspace.


- v0.7.0 source reviewed against LVGL 9.2 and the Espressif JPEG decoder. The earlier generic camera failure did not identify whether HTTP, image dimensions or decoding failed. This revision adds stage-specific on-screen messages and rate-limited logs with HTTP status, byte count and source dimensions. Physical camera playback remains unverified. The code workspace is unavailable, so firmware compilation, C tests and rendered native previews could not run for this revision.


- v0.5.0 compiled under ESP-IDF 6.0.1 and the user confirmed its local build. The user confirmed the v0.6.1 captive portal and direct HA controls work on the board. v0.6.2 addresses TLS allocation failures observed when polling overlaps button commands. v0.6.3 fixes a request-setup regression that prevented the first HA GET from reaching the network, and adds HA startup/failure/connected logs. These source/API changes have been reviewed against IDF 6.0.1 but **have not been compiled or hardware tested in the unavailable coding workspace**. Run the local build above, then repeatedly toggle lights while polling and viewing the camera.
- `tests/native` uses real LVGL on a host with mocked network/RTOS. It renders the same C UI and checks swipe navigation, control routing, target-only changes, stale/expired requests, unknown outcomes, the probe pin guard and return to Climate. It does not validate the physical RGB/touch electronics.
- Direct HA host checks exercise the production C client with a fake transport: exact eleven entities, bearer header, service paths, filtered attributes, missing/stale data, unsupported features, request deduplication, unknown outcomes, camera size request and authentication backoff. A separate empty-token test proves no HA requests are sent without configuration. No real cottage HA token is available to the coding environment; the user has confirmed direct ESP connectivity; the v0.6.2 connection reuse changes still require local validation.
- Existing browser checks continue. The earlier PC bridge remains an optional test helper, but is not used by the ESP.

Hardware/API sources: [Waveshare 4B examples](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4B/ESP-IDF/), [Waveshare BSP](https://components.espressif.com/components/waveshare/esp32_s3_touch_lcd_4b/versions/2.0.0), [LVGL 9.2](https://docs.lvgl.io/9.2/).

## Touchscreen settings and camera sizing (v0.9.0)

The header Settings button opens a local touchscreen dialog. Volume ranges from 0 (muted) to 100 and is saved when the slider is released; it persists across restarts. Test sound uses the selected volume, and laundry notifications keep their independent one-cycle toggles. Network provisioning opens automatically when Wi-Fi is unavailable; there is no network setup button in the touchscreen Settings dialog.

Camera snapshots now enlarge as well as shrink to fit a larger viewport. The default Fit option preserves the whole image with only the borders required by its aspect ratio. Select Fill in Settings to cover the viewport and crop the edges instead; Fit/Fill is retained for this boot. Camera reads pause while Settings covers the display. After 60 seconds without interaction, Settings closes and the display returns to Climate.

Use COM14 for this device when flashing/monitoring. This revision was source-reviewed; firmware compilation and physical testing must be run locally.

## Camera watchdog fix (v0.9.1)

The camera viewport now uses rectangular clipping. Rounded child clipping in LVGL 9.2 creates temporary ARGB layers in its private heap; the observed watchdog backtrace was stuck allocating such a layer. Removing that mask keeps Fit/Fill scaling and the enlarged viewport on the direct image path. The watchdog remains enabled. The native fixture now renders actual 320×180 RGB565 frames in Fit and Fill modes, including repeated frame replacement, rather than only an empty Camera page. Source review only for this revision: the execution environment is unavailable, so run the native fixture and the COM14 hardware build locally.

## Automatic network setup (v0.9.2)

Removed the manual Open network setup button and its touchscreen instructions. Settings now contains volume, Test sound and camera Fit/Fill. Captive portal recovery is unchanged: immediate without saved credentials, or after 30 seconds disconnected when credentials are configured. This is Wi-Fi recovery; a Home Assistant connection failure alone does not open the portal. Firmware compilation and the native UI checks remain pending local execution.


## BMP280 local temperature test (v0.9.3)

Connect a 3.3V BMP280 breakout to rear H3: VCC to pin 4 (3.3V), GND to pin 19/20, SDA/SDI to pin 1 (GPIO47), and SCL/SCK to pin 3 (GPIO48). For a six-pin module, CSB must be high (3.3V) for I2C; strap SDO low for 0x76 or high for 0x77 unless the breakout already provides those straps. The reader checks both addresses and detects BMP280 chip ID 0x58 or BME280 chip ID 0x60. Both support the same factory-calibrated temperature measurements.

From the ESP-IDF PowerShell terminal at repository root:

```powershell
.\tools\build-room-4b.ps1 -Bmp280Test -Flash -Monitor -Port COM14
```

The switch selects probe type 3 and SDA47/SCL48 in your ignored local sdkconfig. It does not change shared defaults. The selection persists for later builds; use `-Configure` and set Probe to 0 to disable it, or choose an SHT sensor type to switch back. Missing hardware is retried every 15 seconds without preventing the display, Wi-Fi or Home Assistant from starting.

The BMP280's factory-calibrated temperature is read every 15 seconds in forced mode, with pressure measurements disabled. Look for `BMP/BME280 TEST: ... F (... C)` in the monitor, the main Climate temperature dial, or Settings' BMP/BME280 test line. Keep the sensor away from board/display heat. Pressure and humidity are not displayed.

All configured local probes now read and update the dial independently of controller pairing, controller availability, Home Assistant and Wi-Fi. Readings older than 30 seconds clear to unavailable. Controller target controls still require fresh controller status. With no local probe configured, the existing controller temperature display remains in use.

BMP280 test readings are never posted to the controller. SHT31/SHT4x reporting uses the configured controller address without a local access token. No furnace arming or servo behavior changes.

Validation: production BMP280 driver compiled with host GCC and passed register-fixture tests under AddressSanitizer/UndefinedBehaviorSanitizer (LeakSanitizer disabled because the container blocks process inspection). Tests cover both addresses, Bosch compensation, chip ID rejection, invalid calibration, forced conversions, bounded status polling, missing/read-failing devices, stale sentinel, range validation and recovery. The native LVGL 9.2 fixture also compiled and passed, covering offline local readings, stale sample clearing, target controls and the existing UI/command/camera checks. Host RGB888 tests use a 1 MiB LVGL pool; firmware memory settings are unchanged. Full ESP-IDF build and physical sensor validation remain local.

## Home icons and sensor diagnostics (v0.9.4)

Blinds now use a locally drawn shade icon: slats gather when open, extend when closed and show an intermediate symbol while moving. Kids heat uses a radiator icon, grey when off and amber with heat rays when on. Kids temperature is a separate 28px reading (previously 14px) with high contrast when available; unavailable data shows --. These visuals use existing LVGL primitives rather than additional font symbols or image buffers.

BMP280 discovery now logs per-address I2C chip-ID read errors, unexpected IDs (including BMP180 0x55), reset/status failures and invalid calibration. Detailed discovery logs are limited to one attempt per minute; retries continue every 15 seconds. BMP280 (0x58) and BME280 (0x60) are supported. An unexpected sensor ID is not reset or treated as a successful temperature reading. The existing generic failed-sample message alone cannot distinguish a disconnected sensor from an unsupported ID; capture the detailed address/ID diagnostics after updating.

The production BMP280 host tests pass with diagnostics checked for mismatched ID and device registration failure. Native LVGL rendering and tests cover the new icons, open/closed and heater on/off visuals, and the larger temperature font. Full ESP-IDF build and physical wiring tests remain local.

BME280 users can enable the same test with the new alias:

```powershell
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

The existing `-Bmp280Test` option and previously saved probe type 3 also work; discovery identifies either model. Only temperature is read in this test, with BME280 humidity and pressure disabled. Existing local/offline temperature behavior is unchanged. Host checks explicitly cover both accepted IDs and reject BMP180 without resetting it.

## SHT45 primary room probe (v0.13.6)

Power down before replacing the BME280. Connect the SHT45 breakout's 3.3V-compatible supply input to rear H3 pin 4 (3.3V), GND to pin 19/20, SDA to pin 1 (GPIO47), and SCL to pin 3 (GPIO48). Follow the breakout's printed signal labels; cable colors and connector order vary. This is the same BSP-owned I2C bus used by the display's touch controller. The SHT45 is read at address 0x44, 100 kHz. Do not apply servo power to the sensor.

From the repository root in the ESP-IDF PowerShell terminal:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Sht45 -Flash -Monitor -Port COM14
```

`-Sht45` replaces the saved BME280 selection with primary SHT4x mode (`CONFIG_ROOM_SENSOR=2`) on SDA47/SCL48. Do not combine it with `-Bme280Test`. Selection persists in `touchscreen/sdkconfig.4b`; later builds without either switch keep that selection.

The startup log identifies `SHT45/SHT4x primary probe configured: address 0x44, SDA47/SCL48`; this confirms configuration, not detection. Successful readings follow every 15 seconds as `SHT45/SHT4x room ... F (... C); report acknowledged` (or `not acknowledged` when the controller cannot accept a report). A valid reading updates the main temperature dial and MQTT room temperature even without controller connectivity. Failed/CRC-invalid reads print `No fresh valid probe sample; no temperature report sent`; readings expire after 30 seconds.

Keep the controller in Hold for the initial sensor/communication test. The SHT45 is the primary control probe, so valid samples are sent to the controller, unlike BME280 test readings. Confirm Local sensor shows Fresh, the controller receives source=room, and HA room temperature agrees. Warm the sensor gently by hand and confirm the reading responds, then let it settle away from display/CPU heat. No encoder or servo movement is needed to test temperature sensing.

For a remote sensor, validate using the intended final cable length as well as short bench leads. This firmware uses ordinary I2C; a successful short-wire test does not establish long-cable reliability. Missing hardware is retried without blocking other services.

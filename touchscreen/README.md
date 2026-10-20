# Cottage room panel — Waveshare ESP32-S3-Touch-LCD-4B

Version 0.4.0 adds the **native 480×480 LCD and capacitive touch interface**. This is for the **4B**, with 16 MB flash and 8 MB octal PSRAM. It uses Waveshare BSP 2.0.0, LVGL 9.2.0 and ESP LVGL port 2.6.2, compiled with ESP-IDF 6.0.1. The dependency lock pins the tested graph. No device has been flashed or physically verified by the coding environment.

## Native pages

- **Climate** is the default. Shows the controller's reported temperature, effective target, operating mode and reason. Adjust in 0.5°F steps with −/+, then tap **Set target**. That writes only `target_f` (45–80°F); it cannot arm the servo, switch operating mode or start calibration.
- **Home** provides explicit Deck/Breezeway On/Off, blinds Open/Stop/Close, Kids Heat On/Off with its separate temperature, and vacuum Start/Stop. Unsupported or unavailable requests are disabled.
- **Laundry** shows the four status sensor strings, without invented progress or completion estimates.
- **Camera** fetches the bridge's scaled JPEGs only while visible, up to one per second. Old images hide on failure, stale data or navigation. An already-running read may finish within its timeout, but its result is discarded after a page change; no further frames are requested. Full-motion video remains separate work.

Swipe left/right or tap the bottom navigation. After 60 seconds without navigation or a control tap, it returns to Climate and stops camera requests. Network work runs in separate tasks; the display task performs no network I/O. Reads refresh every 3–5 seconds; controls disable when the last successful read is more than 10 seconds old. One pending user request is allowed, expires after two seconds if the worker has not begun it, and is never automatically replayed. An uncertain command result asks you to inspect state before intentionally trying again.

The browser interfaces remain at `/` (Climate) and `/home` (HA). On the ESP both require the locally configured room pairing token for API access. The native panel uses the same configured upstream credentials directly, without a pairing screen.

## Windows build and first hardware test

1. Update the `feature/thermostat-foundation` checkout. In a **VSCode ESP-IDF 6.0.1 terminal**, from repository root:

   ```powershell
   .\tools\build-room-4b.ps1 -Configure
   ```

   This creates an ignored **touchscreen/sdkconfig.4b** and **touchscreen/build-4b/**, separate from the old browser-only configuration. Existing Wi-Fi/token settings from `sdkconfig` are **not automatically copied**. Set the fields under **Cottage room thermostat**:

   | Setting | Value |
   |---|---|
   | Wi-Fi SSID / password | LAN Wi-Fi that can reach the bridge and furnace controller |
   | Controller URL | Actual controller LAN URL; leave the default if testing HA alone |
   | Controller pairing token | Its 32-character token; optional for HA-only testing |
   | HA bridge URL | `http://<PC-or-LXC-LAN-IP>:8087`, no trailing slash |
   | HA bridge client token | The separate bridge token printed by the running bridge, **not the HA access token** |
   | Native Waveshare 4B display | Enabled |
   | Rotate display and touch 180 degrees | Enable if required by your mounting |
   | External probe | Keep disabled until wired and confirmed |

2. For a Windows-hosted bridge, restart it with LAN binding:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\ha_bridge\start-local.ps1 -BindAddress 0.0.0.0
   ```

   Allow its port on the private LAN if Windows Firewall prompts. The ESP cannot reach `127.0.0.1` on the PC. A newly generated bridge token changes on restart; keep that bridge process running during the test, or configure a stable `COTTAGE_BRIDGE_TOKEN` privately in the bridge process environment. If the token changes, update the ESP's menuconfig before rebuilding. Keep the HA access token only on the bridge host.

3. Connect the **4B display**, identify its COM port, then explicitly flash:

   ```powershell
   .\tools\build-room-4b.ps1 -Flash -Monitor -Port COMxx
   ```

   Replace `COMxx` with its actual port. The script builds without flashing unless `-Flash` is specified. Serial monitor exits with Ctrl+]. The display DHCP hostname is `CottageThermostat`.

4. Confirm the screen orientation, touch alignment and all four pages. Test Deck On/Off first, then the remaining HA controls. Open/leave Camera and confirm the old picture disappears. Stop the bridge and check that Home controls disable. With no furnace controller connected, Climate correctly shows unavailable values while HA pages remain usable. Verify live camera colors, touch responsiveness and absence of RGB artifacts on the physical unit.

## Probe and controller separation

The native 4B uses the board I2C bus on **SDA GPIO47 / SCL GPIO48** for touch and the IO expander. An optional SHT31/SHT4x probe must share that bus at address 0x44 using 3.3V-compatible wiring; select its type and these pins in menuconfig only after confirming wiring. Other GPIO selections are rejected for the probe in native mode, preventing accidental reuse of LCD pins. The BSP owns I2C initialization. No secondary bus is initialized over display pins.

Valid CRC-checked room samples go directly to the independent controller every 15 seconds and are never queued for later replay. Kids temperature remains display-only and is never reported as a furnace temperature. The controller retains its existing simulation/bench protections and settings. **Servo movement and mechanics remain unverified.**

The browser-only fallback is available by disabling the native display setting; its generic probe pins must then be configured explicitly. Native builds require the 16 MB/PSRAM hardware profile in the new defaults; do not flash an old 8 MB browser build expecting a native LCD.

## Validation

- Native room image compiles under ESP-IDF 6.0.1; 4 MB application partition leaves substantial space.
- `tests/native` uses real LVGL on a host with mocked network/RTOS. It renders the same C UI and checks swipe navigation, control routing, target-only changes, stale/expired requests, unknown outcomes, the probe pin guard and return to Climate. It does not validate the physical RGB/touch electronics.
- The existing controller/browser and bridge tests continue separately. No live HA commands are sent by automated checks.

Hardware/API sources: [Waveshare 4B examples](https://docs.waveshare.net/ESP32-S3-Touch-LCD-4B/ESP-IDF/), [Waveshare BSP](https://components.espressif.com/components/waveshare/esp32_s3_touch_lcd_4b/versions/2.0.0), [LVGL 9.2](https://docs.lvgl.io/9.2/).

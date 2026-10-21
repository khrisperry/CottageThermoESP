# Connectivity and climate review — 2026-10-21

> Latest update: [Panel v0.11.0 / controller v0.4.0 — dashboard, ships and OTA](DEVICE_DASHBOARD_SHIPS_OTA.md). That document supersedes the original versions, status colors, reserved ship service, compile-only integration setup and partition sizes below.

Reviewed baseline: `main`, merge `8a60e47`, controller v0.2.0 / panel v0.9.4. This review branch adds **controller v0.3.0 / panel v0.10.0**. Waveshare ESP32-S3-Touch-LCD-4B remains the planned production display. No device was flashed by this work.

## Review the screen first

Build the panel normally; ESP-NOW defaults off and MQTT defaults unconfigured. Existing saved Wi-Fi, HA URL/token, sound volume and controller address remain in NVS. Local probe reading, independent laundry notifications, camera Fit/Fill and optimistic light controls remain available.

```powershell
git fetch origin
git switch feature/connectivity-climate-review
git pull --ff-only
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

`-Bme280Test` is optional, for the current test sensor. It still never supplies control readings to the furnace. The primary SHT31/SHT4x probe continues to report independently of HA. Flash the v0.3.0 controller before testing thermostat communication: the new panel uses its `/api/link` endpoint. The older controller browser/API remains supported by the controller.

The top bar has a colored, tappable status pill and a gear-only Settings button. Tap the pill for individual Wi-Fi, HA, heat-controller, radio, local-probe and ship-tracking health. Settings retains volume, sound test and camera Fit/Fill.

| State | Color | Meaning |
| --- | --- | --- |
| Online | Green | HA and controller replies are fresh over Wi-Fi |
| HA Offline | Yellow | Controller available; HA unavailable or unconfigured |
| Heat Offline | Yellow | HA available; controller unavailable |
| ESPNOW | Orange | Fresh controller replies through the direct fallback |
| Offline | Red | Both HA and controller unavailable; local functions remain |
| Ship Tracking Offline | Yellow | Reserved for a configured ship service with failed health |

Priority is local-only Offline, confirmed ESPNOW, Heat Offline, HA Offline, then ship failure. A `+N` count exposes additional faults; diagnostics lists them individually. Wi-Fi loss also counts as a fault. Fully offline operation uses a dark red background. Ship tracking currently truthfully says **Not configured**; there is no ship service or invented health check in this release. A test covers the future configured-ship failure state.

## Pair the direct fallback

Configure **both** devices using their local menuconfig (`tools/build-room-4b.ps1 -Configure` for the panel, `idf.py menuconfig` in `controller` for the AtomS3 Lite):

1. Enable **Cottage direct fallback → Enable paired ESP-NOW fallback**.
2. Set each device's peer to the other device's **station MAC**, printed by the Wi-Fi startup log. Do not use its SoftAP MAC.
3. Generate one private random 128-bit key (32 hex digits) and enter that same key on both. For example, run `python -c "import secrets; print(secrets.token_hex(16))"` locally. Do not commit the key or sdkconfig.
4. Keep the existing controller HTTP pairing token configured on the panel for normal Wi-Fi communication. The radio key is separate; the HA REST token never goes over this link.
5. Rebuild/flash both devices and review their startup logs before outage testing.

Radio traffic uses paired encrypted unicast plus an application HMAC. Only status, validated target changes and fresh primary-probe samples are supported; raw servo movement, arming and synchronization are absent. All target writes use the existing persistent controller config validation and output interlocks. Boot still starts with PWM off and unknown position; transport recovery cannot arm or synchronize it.

Wi-Fi remains preferred. A read-only status probe may try HTTP then radio. After a radio reply, the panel displays ESPNOW and disables HA controls/readings and camera requests. A successful controller status over Wi-Fi switches back; HA controls require fresh entity reads after recovery. A mutation with a lost reply is never automatically retried over the other transport. Both transports share client/sequence IDs, a controller boot nonce and a short controller-clock lease; duplicates and expired commands are rejected. No wall clock or NTP is needed for radio control.

Disconnected Wi-Fi owners search channels 1–11 at different dwell intervals, then hold a channel after an authenticated exchange. A connected station stays on its AP's channel. Reconnect attempts are bounded to five seconds with fallback enabled and continue indefinitely; the known working radio channel is restored between attempts. Automatic captive provisioning still opens after an extended Wi-Fi failure. **Channel search pauses while a setup/bench AP client is attached** to preserve provisioning; disconnect that client before cross-channel outage testing. Initial rendezvous/recovery time depends on scan/association timing and needs physical measurement, including one-sided failures and multi-AP networks.

## Discover the thermostat in Home Assistant

The controller publishes one MQTT climate device with a stable identity derived from its station MAC. This is separate from the panel's direct HA REST connection and needs a broker plus HA's MQTT integration.

Copy `controller/secrets.h.example` to ignored `controller/secrets.h`, set broker URL and broker username/password, and rebuild the controller. The existing HA long-lived REST token is **not** an MQTT password. Blank broker URL disables this integration. `mqtt://` supports a local broker; `mqtts://` validates using the certificate bundle and waits for valid system time. No broker credentials are published or logged by application code.

Discovery and state use `homeassistant/climate/<stable-id>/config` and `cottage/thermostat/<stable-id>/...`. Discovery is retained and republished on reconnect and HA birth. State includes Fahrenheit temperature/target, mode, preset, sensor freshness, simulation flag and controller reason. Availability requires both a live broker session (retained Last Will) and a fresh accepted control sensor. A local-only BME280 test does not make this climate available. Missing temperatures clear rather than retaining an old value.

HA can set targets from 45–80 F, choose Heat/thermostat adaptation, or choose the Hold preset. These use the same controller config validation as local controls. Retained, duplicate-flagged, fragmented and queued commands older than five seconds are rejected. The integration deliberately exposes no Off or burner action: Hold stops adaptation, while the physical knob may still call for heat. Unsupported bench modes publish an unknown HVAC mode and no preset. Simulation remains visible in entity attributes. HA commands cannot override the existing simulation/bench arm/calibration/synchronization requirements.

## Validation and remaining hardware checks

Passed in the coding environment:

- ESP-IDF **v6.0.1**, Xtensa GCC **15.2.0**: both firmware projects compile/link and pass app partition checks with ESP-NOW enabled; controller build also includes configured MQTT. Credentials/MACs used for compilation were dummy values, with no flashing or broker connection.
- Controller configured build: approximately 0xEF730 bytes in a 0x100000 app partition (6% free). Native 4B build: approximately 0x192E40 bytes in a 0x400000 partition (61% free).
- Two shared-engine/protocol tests and fourteen native host tests pass. These include real LVGL pages, diagnostics, gear, fallback-disabled controls, stale local readings, HA cache invalidation/recovery, packet validation, expiry/replay/boot change, ambiguous no-retry, a simulated hour outage/recovery, discovery payload, retained-birth rediscovery and MQTT session/command guards, camera, audio and sensor regression checks.
- The isolated Linux build environment needed a component-manager parent-PID workaround because its `/proc` omits transient subprocesses. This is an environment-only change, not a repo/ESP-IDF firmware patch or a requirement on Windows.

Still required before enabling fallback for unattended use:

- [ ] Pair both real boards; verify correct/wrong keys, station MACs and status acknowledgements.
- [ ] Verify live primary probe samples and targets on both transports; BME280 remains a local test only.
- [ ] Remove Wi-Fi for one hour, then restore it; measure handover gaps and automatic recovery.
- [ ] Test one-sided connectivity loss, different AP channels, setup-client attachment and either board rebooting during fallback.
- [ ] Confirm lights/blinds/kids heat/vacuum/camera disable and recover with fresh HA state; local sound/probe continue.
- [ ] Test live MQTT discovery, HA restart, broker restart, Last Will, stale-sensor unavailability, target validation and stable entity identity.
- [ ] Continue the existing inert servo/mechanical and permanent sensor validation; this work does not establish physical actuator behavior.

Reproduce host checks after resolving the panel's managed dependencies:

```sh
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
cmake -S tests/native -B tests/native/build -G Ninja
cmake --build tests/native/build
ctest --test-dir tests/native/build --output-on-failure
```

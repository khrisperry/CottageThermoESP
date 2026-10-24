# Decision record and resume checklist

## Firm decisions

- Repository is **khrisperry/CottageThermoESP**, shared codebase for *both* furnace servo controller and wall-touchscreen firmware.
- Use **native ESP-IDF**, not ESPHome, for production design. Windows VSCode + ESP-IDF; prefer local builds.
- Williams direct-vent furnace model **1773512** with knob levels **0–6**; travel estimated ~120° pending calipers.
- **TS90MD servo first**; MG995 exists but is bulkier. Direct 1:1 coaxial drive using correct shaft-matching spline horn; **no intermediate gears**. Friction-fit cap and detachable rigid servo bracket, mechanical manual override.
- Do not implement backup battery now. After power loss, existing mechanical knob setting should be unchanged and controller must not drive toward OFF on boot. Hardware retention and servo twitch must be tested, not assumed.
- Independent stand-alone firmware with optional Home Assistant Climate/MQTT integration. Planned production display: Waveshare ESP32-S3-Touch-LCD-4B (480×480, 16 MB flash, 8 MB PSRAM), with a remotely mounted room temperature sensor; permanent sensor choice remains open.
- Normal control temps **only** from touchscreen thermostat probe (primary) and existing SwitchBot Hub 3 temperature via HA Matter (secondary, intermittently flaky). If neither valid, no adaptive movement; retain verified last setting and provide independent freeze alarm.
- Cottage Kids sensor measures room with independently controlled radiator, **not** a normal Williams controller input. Can be learning-only context in confirmed unoccupied/door-open periods. Keg sensor is inside kegerator, **never use**.
- HA existing furnace vibration sensor reports detected vibration, heater runtime/ratio and battery (25% in last screenshot); valuable imperfect heating signal. Met.no HA outdoor weather provides ambient temp/forecasts; future outdoor probe optional.
- Empirical temp calibration: dummy examples 1=52°F, 2=59°F, 3=67°F, interpolate fractional knob. Learn real values vs outdoor conditions over longer stable periods.
- Passive observations any time; **active calibration ONLY when cottage is confirmed empty**, explicitly enabled, not heading to cottage and with all safety prerequisites passing. Unknown presence = no calibration.
- Heating time predictor learns from start temp, end temp, outdoor weather, dial and activity. HA future "Heading to Cottage" uses phone location/travel time to start preheat early enough.
- **Open, password-free Wi-Fi for test-device captive portals by default.** Add a captive-portal password only if explicitly requested. This is appropriate for attended bench devices only; revisit access control before a permanent connected actuator installation.
- **First bench microcontroller selected: M5Stack AtomS3 Lite** (ESP32-S3FN8, 8MB flash), servo signal on **rear G38/GPIO38**. Use external regulated servo 5V and shared GND. Firmware source and instructions: `controller/`.
- Priority now: prove servo can move a knob surrogate freely, print trial caps ~18–25 mm, develop simple ESP-IDF servo web bench tool, and later get caliper photos/scan.

## Unresolved / decisions needed

1. Can an external rotating actuator be installed without violating the Williams appliance requirements, and what HVAC review/independent safeguards are needed? **Gate before furnace mounting.**
2. What are exact knob OD/profile, depth, allowable rotational limits, resistive torque and mount thermal envelope?
3. What exact TS90MD shaft spline is installed; compatible T20/T25 hub? Exact PWM endpoints? Safe servo stall/current ratings?
4. Bench board now **AtomS3 Lite on G38**; final production controller hardware still open. What permanent servo wiring/power arrangement will satisfy worst-case stall and furnace-adjacent constraints?
5. How will sensor magnet/absolute angle encoder coexist with coaxial servo shaft, and how will removable-coupler reattachment be recognized?
6. What permanent stationary mounting points are available without blocking service or vents?
7. How to provide standalone backup temperature sensing when touchscreen is offline and HA / SwitchBot Hub 3 isn't available? Consider directly readable BLE sensor later.
8. HA exact entity IDs for hub temp, vibration/run time, outdoor weather, occupancy and person's phone tracker; do not guess.
9. HVAC data: switchbot location; furnace vibration ratio window & reliability; when kids door is open; heat loss/airflow patterns.
10. Best independent low-temperature/freeze alarm arrangement for unattended cottage.
11. How to avoid high write wear for minute-level samples (buffered storage, retention), and verify long-run OTA rollback/watchdog behavior.

## Checklist at the next cottage visit

- [ ] Straight-on and side-on knob photographs with calipers visible.
- [ ] Photograph control knob at a few known levels and measure arc travel without force.
- [ ] Record knob OD (max/min), height, cup grip depth, trim clearance and any ribs/knurl.
- [ ] Scan knob + nearby fascia with scale calibration.
- [ ] Identify acceptable stationary mounting and USB/power route without blocking vents/timer.
- [ ] Verify actual servo spline/horn attachment and measure servo current/torque on separate bench fixture.
- [ ] Print six cap fit coupons; record printer/material, actual ID, fit and removal effort.
- [ ] Bench-test firmware PWM motion then remove power while servo holding *inert* load; note post-reboot displacement.
- [ ] Obtain exact HA entity IDs and check sensor freshness/history.
- [ ] Validate actual unoccupied/occupied HA status before enabling any learning/calibration actions.

## Planned follow-ups — Wi-Fi fallback and HA monitoring

These requirements were originally deferred after v0.9.4. The user authorized implementation for review on 2026-10-21. Software is now on `feature/connectivity-climate-review`; see CONNECTIVITY_REVIEW.md. Unchecked items below remain the end-to-end hardware acceptance checklist, rather than a claim that the software is absent.

- [ ] **ESP-NOW fallback between room panel and furnace controller.** Keep Wi-Fi as the standard transport. When the Wi-Fi/controller link is unavailable, use a securely paired ESP-NOW link for fresh local temperature reports, target changes, acknowledgements and controller status. Coordinate radio channels and discovery, including when only one unit loses Wi-Fi.
- [ ] **Visible fallback state.** Show an explicit persistent **ESP-NOW** communication indicator; distinguish a working fallback from a controller that is unreachable on both transports.
- [ ] **Disable Home Assistant features during fallback.** Grey out/disable all HA-dependent lights, blinds, kids heat controls and vacuum controls; mark HA readings unavailable and stop camera requests. Keep local temperature and valid direct thermostat functions available.
- [ ] **Unified header status indicator and diagnostics.** Replace the separate HA Online/Heat Offline labels with a compact tappable status indicator. Required states: Online (green), HA Offline (yellow), Heat Offline (yellow), ESPNOW (orange), Offline/local-only (red), and Ship Tracking Offline (color to finalize; yellow suggested). Consider a dark red screen background for fully offline operation while preserving readable local temperature and controls. Tapping the indicator opens a settings/debug view with individual Wi-Fi, HA, controller, ESP-NOW, ship-tracking and local-sensor health, so multiple simultaneous failures remain visible. Finalize priority/aggregation of simultaneous faults when designing this feature; a highest-priority pill with an additional-fault count is a proposal. Unimplemented/unconfigured services must be identified truthfully rather than reported as connected or failed.
- [ ] **Gear-only Settings button.** Keep the top-right settings action but remove the word Settings and use a gear icon. Implemented in the review branch; physical UI review remains pending.
- [ ] **Automatic recovery.** Continue checking for Wi-Fi recovery, confirm the controller link is healthy, synchronize current state and return to Wi-Fi. Re-enable HA features only after their connection and data are fresh. Use shared command IDs, acknowledgements and expiration across transports so an old or duplicate command is not applied during handover.
- [ ] **Home Assistant Climate integration (current decision).** Display 0.12.1 owns MQTT and discovers a target-adjustable Climate entity plus temperature/target/controller sensors. HA target requests use the touchscreen thermostat queue; HA has no direct servo or mode commands. Controller 0.4.1 removes its MQTT client/commands. Thermostat operation stays independent of HA/broker. Software tests pass; verify discovery and connectivity on hardware. See [standalone MQTT monitoring](STANDALONE_MQTT_MONITORING.md).
- [ ] **Validation before enabling fallback.** Test an hour-long Wi-Fi outage and automatic recovery, one-sided connectivity loss, restart during fallback, duplicate/expired commands, stale local readings, HA UI disable/re-enable and HA Climate target requests/discovery/availability and standalone operation without HA/MQTT.

## End-of-session handoff template

Update this file with: date, branch/commit SHA, hardware board and wiring, firmware versions, new measurements, test results, unresolved failures, and next command to run. Never replace factual unknowns with assumptions.

## 2026-10-18 software milestone

- v0.2.0: shared thermostat engine, paired controller web/API, NVS settings/maps/schedules, simulation default, conservative bench output interlocks, separate simulation/live stores, source freshness, weekly Detroit schedule, events and six-hour RAM sample export.
- Buildable ESP32-S3 room client with browser UI and SHT31/SHT4x driver. Physical LCD/touch adapter remains pending exact board and wiring.
- Shaft coupon **102** confirmed perfect fit by user; available torque, real knob travel and coupling remain next-weekend tests.
- No active exploration, encoder truth, OTA rollback or predictive/preheat behavior is claimed. MQTT monitoring is implemented for review; physical broker validation remains. See LOCAL_VALIDATION.md for current implementation and remaining work.

## 2026-10-20 Home Assistant milestone

- Room display confirmed: Waveshare ESP32-S3-Touch-LCD-4B.
- User supplied HA HTTPS origin and all eleven entity IDs; authoritative mapping is `ha_bridge/entities.json`.
- HA credentials and camera resize work live on the bridge host (Windows for initial test; Proxmox LXC for permanent hosting). Room firmware receives a small cached model and uses a separate bridge token.
- Kids heat is `input_boolean.kids_heat`; its automation remains in HA. Kids temperature remains display-only for the Williams project.
- Four provided laundry status sensors are displayed as reported, without fictional countdowns.
- First camera implementation is on-demand JPEG live view up to 1 fps; full-motion streaming and physical LCD adapter remain pending.
- HA token and bridge host address are configured locally. No live HA connection or physical device action has been tested by the coding environment.

## 2026-10-20 Native 4B milestone

- User confirmed the HA webpage works with their entities; continue to native display.
- Native Climate/Home/Laundry/Camera implemented using Waveshare 4B BSP 2.0.0, LVGL 9.2.0 and ESP LVGL port 2.6.2. Newest port 2.9.0 did not compile against LVGL 9.2; tested versions are pinned.
- Dedicated 16 MB/octal PSRAM profile uses local sdkconfig.4b and a 4 MB app partition.
- Display network and JPEG decode are off the LVGL task. Camera work stops when leaving the page; idle returns to Climate after 60 seconds.
- Optional external probe shares BSP-owned I2C SDA47/SCL48. Other pins are rejected in native mode.
- Controller servo/mode/arming logic remains unchanged; only explicit target changes are available on the room screen. Ship tracker page/full-motion camera remain future work.
- Firmware compile and host LVGL interaction/layout checks passed. Physical LCD/touch orientation, camera color and live device checks await user flashing.

## 2026-10-20 Direct Home Assistant connection (supersedes bridge requirement)

- User rejected the extra bridge token/process and requested direct ESP access with a long-lived HA token in a secrets file.
- Room panel v0.5.0 reads ignored `touchscreen/secrets.h`; committed `secrets.h.example` contains no token. HTTPS origin defaults to the supplied cottage URL. Existing bridge URL/token menuconfig settings are removed.
- ESP polls only the eleven configured entities, calls allowlisted HA services directly and requests scaled snapshots from HA itself. The earlier PC bridge is optional legacy testing code, not a display dependency.
- The application filters arbitrary attributes/camera tokens from local responses, verifies HTTPS after NTP, refuses redirects and never automatically repeats mutations. Missing credentials make no requests; rejected credentials pause requests until reboot.
- Native HA requires only its HA long-lived token. Existing controller pairing remains separate for furnace/browser access. Kids temperature is still display-only for the Williams controller.
- Direct C client, empty-token and LVGL host checks use simulated requests. No live direct-ESP HA access or physical board action is claimed.

## 2026-10-20 Captive Wi-Fi setup and physical startup follow-up

- User wants captive portals as the default on these ESP devices going forward. This change applies it to the room display; existing other projects are not changed here.
- User's Windows v0.5.0 build succeeded. Physical GT911/LCD initialization then overflowed the main stack; setting it to 16384 bytes allowed native startup. The new defaults and build helper preserve/enforce this minimum for local builds.
- Physical Wi-Fi logs showed repeated authentication/disconnection without a confirmed IP. New code logs the numeric disconnect reason and uses a dedicated reconnect task with backoff.
- Room v0.6.0 adds first-boot/failure setup AP, local DNS/HTTP capture, network scanning, connection testing, NVS save after IP assignment, old-profile retention on failure and an on-screen Wi-Fi setup button.
- HA remains direct with the existing ignored secrets header. No bridge/token requirement was reintroduced; no furnace servo behavior was changed.
- Coding workspace is unavailable. Portal JavaScript was checked with a mock DOM/transport; new C/DNS tests are supplied but have not been run. Firmware compile and physical AP/DHCP/DNS/save/reboot tests remain explicitly pending.

## 2026-10-21 consolidated baseline

- `main` consolidates all 23 feature-branch commits through room panel v0.9.4 and controller v0.2.0. Use `main` for the reviewed baseline.
- The working Waveshare 4B is the planned production display. User confirmed native UI, direct HA, camera, sound, responsive controls and local BME280 testing. Mechanical servo proof and production validation remain pending.
- Follow-up implementation proceeds on a review branch from this baseline; the unchecked list above remains the acceptance checklist until verified.

## 2026-10-21 connectivity review handoff

- `main` contains the consolidated v0.9.4 panel / v0.2.0 controller baseline at merge `8a60e47`; Waveshare 4B is the planned production display.
- Review branch: `feature/connectivity-climate-review`, panel v0.10.0 / controller v0.3.0. Status pill/diagnostics, gear, HA disable/recovery, paired encrypted/authenticated ESP-NOW transport and MQTT climate discovery have software implementations. Both firmware projects compile with radio enabled; configured MQTT controller build also passes.
- Sixteen host tests pass, including real LVGL interaction, packet/expiry/replay, unknown outcome, simulated hour outage, HA cache recovery and discovery payload checks. This is separate from real radio/MQTT/appliance verification.
- Ship tracking is not configured or implemented; diagnostics says so. The future configured-service failure state is represented in status aggregation without inventing a live connection.
- Next: review CONNECTIVITY_REVIEW.md, flash the panel on COM14, then update/pair the controller before physical fallback tests. Keep permanent sensor and mechanical proof on the acceptance list. Firmware does not claim encoder position, burner telemetry or physical Off.

## Current architecture revision — 2026-10-21

The user confirmed standalone thermostat ownership: the wall panel and paired servo controller operate without HA. HA can adjust the wall thermostat target, while having no direct controller mode or servo control. Display 0.12.1 owns MQTT and routes HA temperature requests through the normal touchscreen queue; controller 0.4.1 has no MQTT client. This supersedes older climate-control requirements and review notes. See [standalone MQTT monitoring](STANDALONE_MQTT_MONITORING.md).

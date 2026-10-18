# Decision record and resume checklist

## Firm decisions

- Repository is **khrisperry/CottageThermoESP**, shared codebase for *both* furnace servo controller and wall-touchscreen firmware.
- Use **native ESP-IDF**, not ESPHome, for production design. Windows VSCode + ESP-IDF; prefer local builds.
- Williams direct-vent furnace model **1773512** with knob levels **0–6**; travel estimated ~120° pending calipers.
- **TS90MD servo first**; MG995 exists but is bulkier. Direct 1:1 coaxial drive using correct shaft-matching spline horn; **no intermediate gears**. Friction-fit cap and detachable rigid servo bracket, mechanical manual override.
- Do not implement backup battery now. After power loss, existing mechanical knob setting should be unchanged and controller must not drive toward OFF on boot. Hardware retention and servo twitch must be tested, not assumed.
- Independent stand-alone firmware with optional Home Assistant climate/MQTT. Future ESP32-S3 Touch-LCD-4B room thermostat with remotely mounted room temp sensor.
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

## End-of-session handoff template

Update this file with: date, branch/commit SHA, hardware board and wiring, firmware versions, new measurements, test results, unresolved failures, and next command to run. Never replace factual unknowns with assumptions.

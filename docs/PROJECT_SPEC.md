# CottageThermoESP — living design specification

Last established: 2026-10-17. This document is the authoritative handoff of the discussion; update it as experiments resolve unknowns.

## 1. Appliance and operating model

- Appliance: **Williams 1773512** direct-vent wall furnace at the cottage.
- Existing control: physically rotatable thermostat knob marked **0–6**, roughly estimated travel **~120°** (NOT measured); actual travel, geometry and torque must be measured.
- The **Williams' own thermostat** remains responsible for cycling/regulating the furnace. We are NOT replacing internal gas or safety controls.
- Setting 0 may correspond to OFF/shutdown. Treat it as a special, potentially hazardous operation. Normal smart control must never casually rotate to 0; no automatic return-to-zero on boot, OTA, disconnect or fault.
- Never bypass factory combustion protections, vents, sensors or shutdown functions. An external actuator might conflict with manufacturer requirements. Obtain qualified professional review before attachment to the operating gas appliance. A separate low-temperature alert/freeze-monitor path is needed for unattended pipe protection.

## 2. System split

**A. Furnace/controller ESP32, native ESP-IDF**
- Owns knob movement, calibration, physical-position truth, temperature-to-knob mapping, thermostat schedule/setpoint, history and learned model.
- Provides local web UI, provisioning, logs, OTA with rollback, Home Assistant integration via MQTT Discovery, authenticated direct client API.
- Must operate without Home Assistant; uses independently available temperature information when deployed. Store configs/model and target persistently in NVS/versioned records.
- When current room-temperature data are unavailable: do NOT invent readings or move based on stale sensor values. Preserve last verified safe position, report fault and use independent low-temperature monitoring. Standalone operation with zero fresh temperature sources cannot promise active climate regulation.

**B. Room thermostat/display ESP32-S3**
- Candidate hardware: **Waveshare ESP32-S3-Touch-LCD-4B** (same family previously used for touchscreen projects). Native ESP-IDF, LVGL UI.
- Shows current indoor temperature, target temperature, operating mode, dial level, furnace activity, signal/health, predicted ready time; change setpoint by touch.
- Use a remotely mounted **SHT31/SHT4x** or similar temperature sensor away from LCD/CPU heat; BMP280 is not preferred for room temperature. Display communicates readings and commands directly to controller through Wi-Fi (authenticated local API/WebSocket; BLE optional future fallback/provisioning).
- Display is a UI/client and primary room sensor. Its restarts/updates must not remove the controller's committed thermostat settings.

**C. Home Assistant optional extension**
- Climate entity and related sensor/diagnostic entities; firmware works without HA for normal thermostat functionality.
- HA adds historical context (weather, vibration, occupancy) and trip/location automation. It may connect SwitchBot Hub 3 Matter temperature (sometimes flaky).
- Optional HA "Heading to Cottage" trigger, tracked phone ETA with travel-time integration, compare travel remaining against measured heating duration + uncertainty margin. Latch started preheat, allow cancel/manual override, update progress. No permanent location tracking required solely for controller operation.

## 3. Mechanical design

- **Direct drive 1:1**: stationary bracket/servo enclosure, servo output shaft precisely coaxial with furnace knob, spline-matching horn/hub, detachable coupling, 3D-printed friction-fit cap.
- NO idler gears, spur gears, belts, linkages or servo horn rubbing the panel; prior gear arrangement caused binding.
- **TS90MD** servo (nominal 270°) first candidate; MG995 on hand but physically bulky. Actual usable PWM endpoints/torque and shaft spline must be verified for the installed unit.
- Friction cap stays on original knob when removable servo module is lifted/uncoupled. Manual rotation must be possible without tools and independent of software/power. The servo bracket must resist reaction torque without loading knob shaft sideways. Do not mount over vents or compromise access to original OFF/control functions.
- Trial cap inner diameters **18–25 mm** are speculative; print 4–6 variants and measure fit without forcing. Need knob profile, depth, diameter, rotation, body/panel clearance and bracket anchor dimensions from caliper photos / 3D scanner.
- Consider future magnetic absolute rotary encoder (AS5600 or equivalent) with magnet embedded in cap; **coaxial servo output occupies the same axis**, so magnetic/sensor packaging is unresolved. No positional claims solely from commanded PWM.
- Need evaluate furnace-adjacent ambient temperature before selecting plastic; prototypes away from heat only. PETG/ASA may be candidates subject to measured temperature and professional review.

## 4. Software modes

- **Direct knob mode**: request a dial level within validated bounds, including fractional steps (e.g., 2.5). Level 0 is protected/special, never selected by ordinary calibration routines.
- **Thermostat mode**: request a desired indoor °F. Convert target to knob angle from empirical room-temperature/dial calibration, let Williams regulate, avoid needless servo hunting by hysteresis/dwell/rate limits.
- **Hold**: leave verified knob at current setting, suspend optimization/automatic movements.
- **Manual**: module detached or manually selected; disable actuation, report physical position if sensor present.
- **Fault**: disable automatic movement, preserve mechanical setting, clearly report cause; do not move toward zero.

### Example (made-up, NOT measured)
Level 1 ~ 52°F; level 2 ~ 59°F; level 3 ~ 67°F. Interpolation might map 63°F to level 2.5. Learn actual mapping by holding a dial setting for hours under measured indoor/outdoor conditions. Record uncertainty and weather dependence. No unrestricted extrapolation beyond measured safe range.

## 5. Startup and power recovery — NON-NEGOTIABLE

1. Servo PWM is **disabled at boot**; do not initialize an output that commands an arbitrary angle and do not sweep/home into a stop.
2. Load and validate stored configuration/last target from NVS, but do NOT mistake last command for actual physical position.
3. Read a physical position sensor if installed and credible; otherwise require explicit bench/manual synchronization before allowing movement.
4. A powered servo may jump when first engaged even if software did not command a sweep. A hobby servo can also be back-driven with power off. Validate electrically/mechanically before real attachment.
5. Preserve the existing mechanical knob setting across loss of power; no automatic OFF or reset to 0. Once source freshness and physical position are established, normal operation can resume conservatively.
6. No backup battery in initial design: furnace itself requires mains. A separate, reliable freeze protection alert/safety plan is still needed.

## 6. Temperature sources — accepted/rejected

- **PRIMARY**: future touch thermostat's properly positioned room temperature sensor delivered directly to controller.
- **SECONDARY, OPTIONAL**: existing **SwitchBot Hub 3** temperature via Home Assistant/Matter; flaky sometimes, so apply TTL/freshness/disagreement check. This source disappears if HA/Matter transport unavailable and is not independent.
- **Cottage Kids Zigbee2MQTT temp/humidity**: NOT for normal thermostat control (separately regulated radiator; Williams does not heat that room with door shut). May provide learning context ONLY if cottage unoccupied AND door known open; never use it as sole controller reading.
- **Keg temperature Zigbee2MQTT**: NEVER use; sensor is inside kegerator.
- **Furnace vibration**: Home Assistant reports vibration, Heater Run Time and Heater Running Ratio. Useful as uncertain evidence of furnace/fan activity; validate meaning/window. Sample earlier: runtime 9h21, ratio 39%, battery 25% at time of screenshot.
- **Outdoor**: Home Assistant Met.no forecast (observed and hourly predicted temperature, possibly wind) to explain heat loss/recovery time. Optional future local outdoor probe. Weather absent must not break local thermostat.

Do not average different rooms or silently substitute stale/suspect sensors. Track source, timestamp, quality, placement and disagreements.

## 7. Learning and preheat

Two distinct models:
1. **Steady-state map**: dial position + outdoor conditions -> resulting stabilized indoor temperature.
2. **Recovery model**: starting indoor temp, target, outdoor temp/wind if available, furnace activity proxy + historical ramp -> predicted time to target and confidence range. Log heating events, decay periods, knob changes, temperature samples ~once/minute, vibration state changes, source freshness.

**Separate learning vs adjustment**:
- Passive observation may continue in occupied mode.
- Active exploration/calibration (deliberately changing knob to learn response) ONLY with explicit enable, confirmed HA unoccupied status, no people/arrival trip, validated mechanical/sensor conditions, conservative temperature bounds, and appropriate safety approvals. **Unknown occupancy = not eligible.**
- "Heading to Cottage" immediately cancels active calibration and changes to bounded preheat.
- Support model history, rollback of poor calibration, conservative motion limits and audit logs.
- Future HA action: press "Heading to Cottage", track opt-in phone ETA, compare ETA to recovery time plus margin; initiate preheat only when appropriate; notify if target cannot be reached by arrival.
- Important: learning/preheat convenience does not replace independent pipe-freeze protection.

## 8. Communications / persistence

- ESP-IDF FreeRTOS, LEDC servo PWM, NVS with schema/version/checks, local web UI.
- Direct controller-display API with authentication, idempotent commands and acknowledgements, fresh temperature reports and live status.
- MQTT Home Assistant Discovery for climate and diagnostic sensors, optional broker; independent firmware OTA with rollback, serial diagnostics and local safe UI.
- Event log contains sensor source, readings/age, firmware, position measured vs commanded, fault reason, power-restore, movement outcome.
- Calibration persistence must survive ESP32 reboots (unlike issues observed in earlier ESPHome implementation).

## 9. Phased execution and evidence

**P0 - now**: comprehensive project docs, geometry fit coupons, simple AP + web servo bench-test firmware for **M5Stack AtomS3 Lite G38/GPIO38**. No furnace automation.
**P1 - dimensional work**: caliper photos and 3D scan at cottage; verify knob travel/diameter/profile, mounting and safe thermal envelope.
**P2 - bench mechanical proof**: verify TS90MD direct-drive available torque, no binding, no side loading, repeatable detachable coupling, no power-on twitch into load. Test on isolated surrogate, then only proceed near appliance after qualified review.
**P3 - controller firmware**: mechanical-angle feedback, validated safe states, config, sensor inputs, learning logs, local thermostat engine.
**P4 - display**: ESP32-S3 Touch-LCD-4B UI, SHT room probe and secure pairing.
**P5 - HA**: climate, optional SwitchBot/vibration/weather, occupancy gating and preheat ETA.
**P6 - long-term optimization**: bounded adaptive calibration + recovery prediction once enough validated real samples exist.

## 10. Out of scope / unknowns

- Actual original knob diameter, cross-section/grip geometry, height, travel and torque are **UNKNOWN**. Images are too low resolution to measure.
- Servo T20 vs T25 spline depends on actual servo; do not assume compatibility.
- Exact ESP32/controller board, GPIO, servo voltage/current and bracket fastening points not yet selected.
- Temperature ratings, manufacturer approval, installed mechanical fail-safe, absolute encoder placement and actual gas furnace behavior remain unverified.
- **Do not declare this safe for unattended occupancy or freeze-protection until all relevant reviews, tests and independent safeguards are completed.**

## 11. Implementation update — 2026-10-18

User authorized hardware-independent thermostat/controller work before next-weekend mechanical proof. v0.2.0 implements the map-based engine, persistent settings, weekly schedules, primary/secondary temperature API, paired web controls, simulated automatic output and explicitly armed physical bench commands. Simulation/live maps use separate NVS keys; no fictional calibration ships. The room-client project compiles independently with SHT31/SHT4x support and a browser thermostat; physical LCD/touch integration remains pending exact board/wiring.

This advances the software phases without declaring mechanical proof, measured physical position, installation readiness, trained recovery prediction, MQTT Discovery, OTA or active calibration complete. Current source/build/API behavior is documented in controller/README.md, touchscreen/README.md, API.md and LOCAL_VALIDATION.md. Initial P0 bench instructions above describe the earlier prototype; the v0.2.0 controller now requires a pairing token and explicit bench arm before raw PWM.

# CottageThermoESP

ESP-IDF smart thermostat / mechanical knob actuator project for a **Williams model 1773512 direct-vent cottage furnace**.

> **Review: controller v0.5.3; native 4B room panel v0.13.5.** Controller and room client are separate ESP-IDF 6.0.1 projects. Automatic output defaults to simulation; physical servo commands are explicitly armed bench operations. Mechanical travel/torque, encoder feedback and appliance integration remain unverified.

The consolidated v0.9.4 baseline is on `main`; this follow-up is on `feature/connectivity-climate-review`. See [connectivity review and setup](docs/CONNECTIVITY_REVIEW.md) for the new status/diagnostics UI, paired ESP-NOW fallback, initial MQTT discovery, exact build evidence and pending physical checks. See the latest [dashboard, ship tracker and OTA setup](docs/DEVICE_DASHBOARD_SHIPS_OTA.md) for the aligned status screens, full LAN settings, live ship service integration and first USB migration to webpage updates. The latest [standalone thermostat and MQTT Climate guide](docs/STANDALONE_MQTT_MONITORING.md) describes HA target-temperature requests through the wall thermostat. HA never controls the servo directly; standalone thermostat operation remains independent of HA/MQTT.

Current controller setup and independent link checks: [Wi-Fi, ESP-NOW and Omada outage testing](docs/CONTROLLER_LINK_TESTING.md).

## Start here when resuming the project

1. **[docs/PROJECT_SPEC.md](docs/PROJECT_SPEC.md)** — complete context, requirements, software architecture, sensor selection, learning, Home Assistant, and failure behavior.
2. **[docs/PROTOTYPE_01.md](docs/PROTOTYPE_01.md)** — the first experiment: TS90MD + basic ESP-IDF web control + 3D-printed friction caps.
3. **[docs/MECHANICAL_NOTES.md](docs/MECHANICAL_NOTES.md)** — knobs, direct-drive coupling, cap trial sizes, measurement list and attachment concept.
4. **[docs/DECISIONS_AND_OPEN_QUESTIONS.md](docs/DECISIONS_AND_OPEN_QUESTIONS.md)** — confirmed decisions, unknowns, next session checklist.
5. **[hardware/cap_fit_kit.scad](hardware/cap_fit_kit.scad)** — parametric **test coupons**, not a verified knob/servo coupling.
6. **[controller/README.md](controller/README.md)** — controller setup, modes, calibration and bench movement.
7. **[touchscreen/README.md](touchscreen/README.md)** — native 4B build, captive Wi-Fi/direct HA setup and first hardware checks.
8. **[docs/LOCAL_VALIDATION.md](docs/LOCAL_VALIDATION.md)** — local build/test steps and next cottage visit.
9. **[docs/API.md](docs/API.md)** — direct paired API, schedules and data freshness.

## Home Assistant development

The native room client connects **directly to Home Assistant** using a long-lived token in ignored `touchscreen/secrets.h`. No bridge or bridge token is required. See [touchscreen setup](touchscreen/README.md) for the one-time secrets file, Wi-Fi settings and 4B build/flash commands. Climate/Home/Laundry/Camera and swipe navigation are implemented. The direct-HA v0.5.0 build/checks passed. v0.6.0 adds captive Wi-Fi provisioning, an on-screen Wi-Fi button and a 16 KB startup stack; local compilation and physical portal testing are pending.

## Goal

- Keep the original Williams furnace thermostat and safety functions intact. Its physical 0–6 dial sets the temperature; it is **not** an on/off power relay.
- Fit a removable 3D-printed cap over the existing knob, driven 1:1 by a coaxial servo (starting with **TS90MD**). **No intermediate gears.**
- Servo body stays fixed to a removable bracket; cap and shaft rotate. Cap can stay on the knob for manual turning when servo module is detached.
- Servo must not automatically go to 0, sweep, or seek a default position on reboot / power restoration.
- Ultimately: standalone local thermostat operation, optional Home Assistant Climate integration, ESP32-S3 touchscreen room thermostat, adaptive temperature-to-dial calibration, heating recovery-time learning, and local schedules.

## Software now implemented

Persistent modes/target/calibration, measured-map interpolation, fractional dial commands, source freshness/disagreement checks, movement limits, weekly schedules in Detroit time, direct local web/API access, optional LAN Wi-Fi, events and six-hour sample CSV export. The room client reports CRC-checked SHT31/SHT4x samples and presents browser and native 4B interfaces. Native display electronics and probe wiring await physical validation.

```text
controller/    AtomS3 Lite controller + bench web UI
touchscreen/ native 4B + external probe + browser UI
shared/thermo/ hardware-independent thermostat engine
tests/        engine, LVGL host, HA bridge and browser checks
tools/        Windows local build helper
hardware/     CAD, reference models and tests
docs/         specification, API, validation and engineering decisions
```

Build both locally from the configured ESP-IDF PowerShell terminal:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1
```

## Current short-term milestone: does the servo have enough torque?

Test a **TS90MD**, controlled by an **M5Stack AtomS3 Lite on rear GPIO38 (G38)**, with a secured stationary servo bracket and printed trial cap. Start on a disconnected dummy knob or equivalent load. Establish real knob dimensions, rotation range, friction/torque, and whether the servo moves the knob without binding. The MG995 is available as a larger fallback but not the first choice.

A fitted cap and stationary mount are independent parts. The servo horn/spline must match the **actual servo shaft** (T20 and T25 are not interchangeable); use the correct supplied/matching metal horn or hub rather than guessing a printed spline.

**Project origin:** planning discussion, 2026-10-17. All example calibration temperatures, runtime estimates and cap diameters are placeholders until measured.

## Consolidated display baseline (2026-10-21)

Use `main` for the consolidated controller/display baseline. Waveshare ESP32-S3-Touch-LCD-4B is the planned production display (480×480, 16 MB flash, 8 MB PSRAM). User hardware tests confirm the native UI, direct Home Assistant, camera, sound and local BME280 test sensor. These results supersede older pending-display notes above. Permanent sensing and furnace mechanics still require validation. Follow-up work is reviewed separately before integration.

```powershell
git switch main
git pull --ff-only
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

Omit `-Bme280Test` when retaining the configured permanent sensor profile. Deferred transport, status and climate requirements are tracked in `docs/DECISIONS_AND_OPEN_QUESTIONS.md`.

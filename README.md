# CottageThermoESP

ESP-IDF smart thermostat / mechanical knob actuator project for a **Williams model 1773512 direct-vent cottage furnace**.

> **Status: v0.2.0 thermostat development firmware.** Controller and room client are separate ESP-IDF 6.0.1 projects. Automatic output defaults to simulation; physical servo commands are explicitly armed bench operations. Mechanical travel/torque, encoder feedback and appliance integration remain unverified.

## Start here when resuming the project

1. **[docs/PROJECT_SPEC.md](docs/PROJECT_SPEC.md)** — complete context, requirements, software architecture, sensor selection, learning, Home Assistant, and failure behavior.
2. **[docs/PROTOTYPE_01.md](docs/PROTOTYPE_01.md)** — the first experiment: TS90MD + basic ESP-IDF web control + 3D-printed friction caps.
3. **[docs/MECHANICAL_NOTES.md](docs/MECHANICAL_NOTES.md)** — knobs, direct-drive coupling, cap trial sizes, measurement list and attachment concept.
4. **[docs/DECISIONS_AND_OPEN_QUESTIONS.md](docs/DECISIONS_AND_OPEN_QUESTIONS.md)** — confirmed decisions, unknowns, next session checklist.
5. **[hardware/cap_fit_kit.scad](hardware/cap_fit_kit.scad)** — parametric **test coupons**, not a verified knob/servo coupling.
6. **[controller/README.md](controller/README.md)** — controller setup, modes, calibration and bench movement.
7. **[touchscreen/README.md](touchscreen/README.md)** — buildable room client, probe support and physical LCD adapter status.
8. **[docs/LOCAL_VALIDATION.md](docs/LOCAL_VALIDATION.md)** — local build/test steps and next cottage visit.
9. **[docs/API.md](docs/API.md)** — direct paired API, schedules and data freshness.

## Home Assistant development

The room client v0.3.0 adds a Home/Laundry/Camera browser panel and compact bridge API. [ha_bridge/README.md](ha_bridge/README.md) includes the confirmed entity mappings, local token setup and Windows/Proxmox instructions. The physical 4B LCD adapter remains pending.

## Goal

- Keep the original Williams furnace thermostat and safety functions intact. Its physical 0–6 dial sets the temperature; it is **not** an on/off power relay.
- Fit a removable 3D-printed cap over the existing knob, driven 1:1 by a coaxial servo (starting with **TS90MD**). **No intermediate gears.**
- Servo body stays fixed to a removable bracket; cap and shaft rotate. Cap can stay on the knob for manual turning when servo module is detached.
- Servo must not automatically go to 0, sweep, or seek a default position on reboot / power restoration.
- Ultimately: standalone local thermostat operation, optional Home Assistant climate integration, ESP32-S3 touchscreen room thermostat, adaptive temperature-to-dial calibration, heating recovery-time learning, and HA-assisted "Heading to Cottage" preheat.

## Software now implemented

Persistent modes/target/calibration, measured-map interpolation, fractional dial commands, source freshness/disagreement checks, movement limits, weekly schedules in Detroit time, paired web/API access, optional LAN Wi-Fi, events and six-hour sample CSV export. The room client reports CRC-checked SHT31/SHT4x samples and presents a browser thermostat; exact LCD/touch board integration remains pending.

```text
controller/    AtomS3 Lite controller + bench web UI
touchscreen/ room client + external probe + browser UI (LCD adapter pending)
shared/thermo/ hardware-independent thermostat engine
tests/        native engine and optional browser checks
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

# CottageThermoESP

ESP-IDF smart thermostat / mechanical knob actuator project for a **Williams model 1773512 direct-vent cottage furnace**.

> **Status: concept documented; first milestone is a removable, direct-drive, BENCH-ONLY servo proof of concept. No unattended operation of a modified gas furnace is authorized or validated.** Do not install this on a live gas appliance without appropriate professional review and confirmation that it is permitted by the appliance manufacturer and applicable codes. Do not bypass any original furnace safety controls.

## Start here when resuming the project

1. **[docs/PROJECT_SPEC.md](docs/PROJECT_SPEC.md)** — complete context, requirements, software architecture, sensor selection, learning, Home Assistant, and failure behavior.
2. **[docs/PROTOTYPE_01.md](docs/PROTOTYPE_01.md)** — the first experiment: TS90MD + basic ESP-IDF web control + 3D-printed friction caps.
3. **[docs/MECHANICAL_NOTES.md](docs/MECHANICAL_NOTES.md)** — knobs, direct-drive coupling, cap trial sizes, measurement list and attachment concept.
4. **[docs/DECISIONS_AND_OPEN_QUESTIONS.md](docs/DECISIONS_AND_OPEN_QUESTIONS.md)** — confirmed decisions, unknowns, next session checklist.
5. **[hardware/cap_fit_kit.scad](hardware/cap_fit_kit.scad)** — parametric **test coupons**, not a verified knob/servo coupling.
6. **[controller/README.md](controller/README.md)** — bench controller firmware and build steps.

## Goal

- Keep the original Williams furnace thermostat and safety functions intact. Its physical 0–6 dial sets the temperature; it is **not** an on/off power relay.
- Fit a removable 3D-printed cap over the existing knob, driven 1:1 by a coaxial servo (starting with **TS90MD**). **No intermediate gears.**
- Servo body stays fixed to a removable bracket; cap and shaft rotate. Cap can stay on the knob for manual turning when servo module is detached.
- Servo must not automatically go to 0, sweep, or seek a default position on reboot / power restoration.
- Ultimately: standalone local thermostat operation, optional Home Assistant climate integration, ESP32-S3 touchscreen room thermostat, adaptive temperature-to-dial calibration, heating recovery-time learning, and HA-assisted "Heading to Cottage" preheat.

## Proposed directories

\`\`\`
CottageThermoESP/
  controller/    ESP-IDF furnace-side controller (first bench prototype now)
  touchscreen/   future ESP32-S3 Touch-LCD-4B UI and room-temperature sensing
  shared/        future wire protocol
  hardware/      parametric CAD, measured models and tests
  docs/          requirements, test plans, engineering decisions
\`\`\`

These future directories will be created as implementation begins; see the detailed design documents.

## Current short-term milestone: does the servo have enough torque?

Test a **TS90MD** with a secured stationary servo bracket and printed trial cap. Start on a disconnected dummy knob or equivalent load. Establish real knob dimensions, rotation range, friction/torque, and whether the servo moves the knob without binding. The MG995 is available as a larger fallback but not the first choice.

A fitted cap and stationary mount are independent parts. The servo horn/spline must match the **actual servo shaft** (T20 and T25 are not interchangeable); use the correct supplied/matching metal horn or hub rather than guessing a printed spline.

**Project origin:** planning discussion, 2026-10-17. All example calibration temperatures, runtime estimates and cap diameters are placeholders until measured.

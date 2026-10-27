# Controller v0.6.3: Calibration → Estimated temperatures → Test → Go

**v0.6.3 responsive setpoints:** In attended Go, changes to the thermostat target (touchscreen, Home Assistant through the panel, local target button, or enabled schedule) now select the full calibrated dial position promptly, with a short **2-second** spacing guard for rapid repeated edits, **not** five-minute intervals of 0.25 dial. Ordinary room-temperature changes alone do not cause dial moves. The conservative 0.25-level / five-minute controls remain for non-setpoint adjustments. Go is still explicitly armed for **15 minutes** per attended bench session; startup and Stop disable PWM. The controller Go status now shows commanded dial, requested dial, and the short guard countdown. The LEDC GPIO5 channel is initialized once and subsequently updated through duty commands. Encoder feedback is read-only and does not validate that a command physically reached its destination.



This replaces the controller's JSON-map/simulation/manual-controls page. Use the
Super Mini local build script as before. v0.6.0 removed the 13 dBm cap; v0.6.1 restores it for a controlled Wi-Fi comparison after authentication timeouts at the 20 dBm default. See [Wi-Fi notes](CONTROLLER_WIFI_V061.md).
The touchscreen does not require a firmware update for this controller workflow.

## 1. Calibration

Enter **Test Mode** (15-minute attended bench session). This stops automatic
control and PWM, then permits deliberate manual commands. It does not move the
arm. Send an appropriate starting pulse explicitly, then jog by 1, 5, 10, 25 or
50 microseconds while watching the arm and live angle.

Capture the start and end of usable travel, short of binding. Both may be in
either pulse order. Once both are saved, all PWM commands, including legacy
manual endpoints and Go commands, must stay inside that inclusive range. Until
both exist, the hard electrical command range remains 500–2500 microseconds.
**v0.6.2 correction:** If you captured a boundary too early, do **not** reset
all calibration. Click **Re-measure start limit…** or **Re-measure end limit…**
under the saved boundary. Confirm the attended unloaded bench operation. The
controller immediately stops PWM, cancels any Go/Test session and clears only
that one travel limit; the other limit, captured dial points and reference
encoder angles, and estimated dial temperatures remain saved. Enter Test Mode
again, send a deliberate starting pulse, jog to the corrected endpoint and
click **Capture current start/end**. While one limit is open, only the hard
500–2500 µs range is enforced for attended manual movement, and Go is
unavailable. If the new narrower limit would exclude a previously captured
dial point, recapture that point inside the intended travel before completing
the limits. This workflow is for supervised unloaded bench calibration,
never an unattended mounted furnace. No flash erase is necessary.

Jog to each marking and capture **Off, 1, 2, 3, 4, 5, 6, Max**. Capture records the
pulse actually being sent; typing a number alone does not capture a position.
When the AS5600 reading is fresh and valid, its angle is saved as reference.
Missing encoder feedback never invents an angle. The encoder remains read-only;
this release does not implement closed-loop angle correction or automatic homing.

Captured dial pulses must progress consistently in one direction, including Off
and Max. The UI reports missing or invalid steps. All reference positions must
remain within the travel limits. Off and Max are stored references; automatic
control uses only 1–6 and never selects Off or extrapolates to Max.

## 2. Estimated temperatures

Enter six increasing estimates, one for each numbered dial position, 40–90 F.
Save them together. No synthetic or sample temperatures are installed by this
firmware. Estimates describe the setting of the furnace's own thermostat; they
are not a direct on/off control algorithm based on instantaneous room error.

Old simulation JSON maps are not imported as physical calibration. Existing
Wi-Fi/pairing settings remain. The new calibration has its own NVS record and
survives restart. Updating v0.5.8 does not require first installing v0.5.9.

## 3. Test Mode

Automatic control is disabled. Small jogs require an active session and an
explicit initial pulse. A saved-position move is a deliberate, confirmed action
and may be larger than a jog. The page never moves or arms the servo on load.
Capturing or editing data never selects Go. Entering Test Mode from Go stops
physical automatic movement and clears position synchronization.

## 4. Go Mode

In Test Mode, move to a saved number, 1–6. Select that same number under Go and
verify the physical arm matches it. Set a target within the estimated numbered
temperature range (the thermostat target itself remains limited to 45–80 F).
Go requires a complete ordered calibration, estimated temperatures, a fresh
allowed temperature sample, active bench permission, and PWM currently at the
captured starting number. A known position is never assumed after a restart.

**Go starts physical automatic bench movement**, without changing a compile-time
simulation setting. It builds the control map from the saved numbered positions
and estimates. It retains 0.25-number maximum steps, five-minute movement dwell,
and 0.1-number deadband. The UI shows the dwell countdown instead of an unexplained
`movement_dwell`. Stale readings pause new movements. SwitchBot remains an
optional existing secondary source; no other sensors are added.

Go starts a fresh 15-minute attended session; expiry stops PWM and enters Hold.
Stop disables both manual and automatic movement. Restart always starts stopped,
regardless of the last saved target or mode. A remote target update cannot start
Go or arm motion. This remains a bench workflow, not unattended furnace firmware.

`THERMO_LIVE_BENCH` has been retired; local Go is now the sole route into physical
automatic movement. The shared engine accepts calibrated pulses over 500–2500 us;
controller travel limits constrain actual output. Existing legacy simulation APIs
remain for compatibility, but cannot start physical automatic output.

## Local build / flash

From the repository root in the ESP-IDF 6.0.1 PowerShell terminal:

```powershell
.\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM13
```

Use the actual controller port. Do not erase flash/NVS. Confirm version 0.6.0 and
open the controller webpage, refreshing the page after flashing. Saved network
and pairing settings are kept. Capture actual final calibration after installation.

## Validation

Host tests cover the actual setup HTTP handler with fake PWM/NVS boundaries,
including travel enforcement, capture prerequisites, invalid/incomplete data,
failed persistence, fresh-temperature requirements, explicit Go transition,
manual exclusion during Go, and expired session requests. Core tests cover
reversed direction, full-range map validation, Off/Max exclusion, freshness,
movement bounds and dwell. DOM tests execute the actual page script and cover
no motion on load, unique control IDs, Test/jog/Go/Stop, known starting position,
target updates, invalid temperature entry and offline control disabling.

```sh
cmake -S tests -B build-host -DCJSON_SOURCE_DIR=/path/to/cJSON
cmake --build build-host
ctest --test-dir build-host --output-on-failure
cd tests
npm ci --ignore-scripts
npm run test:controller
```

Browser rendering and a target ESP-IDF build were unavailable in the editing
runtime. The self-contained `controller-workflow-preview.html` uses the actual
page layout with sample data and never sends device requests. Review it in a
browser; physical movement, mounting repeatability, stop/restart and sensor-loss
behavior must be checked on the local bench after building/flashing.

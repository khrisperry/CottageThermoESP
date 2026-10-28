# Cottage Thermostat — v0.7.1 Controller / v0.14.7 Waveshare 4B acceptance

## v0.14.7 — original Design #3 reproduction and acceptance (visual-only)

The initial v0.14.5 refresh used the right palette but retained the old centered
climate layout and text-only navigation. v0.14.7 corrects those visual mismatches:

- Climate has a left-anchored **heat setpoint** in large type, a separate **current**
  room reading underneath, and a RIGHT-anchored **blue/orange 248px drag ring**
  with radial markers. The center hint remains in two lines. All four
  previously approved buttons **-1 / Set Temp / +1 / Away/Home** remain in one
  row. Live/Bench/Service status and reason stay visible below the ring.
- Home has five richer, icon-left/text-right gradient cards in a two-column
  grid. The sixth slot is empty; Camera is NOT on Home. Buttons and HA command
  permissions are exactly those of the established firmware.
- Shared header and bottom navigation use the Design #3 compact online badge,
  thermometer/home/laundry/camera/ships icons and label captions. The page IDs
  and all callbacks remain the same. **Ships stays visible and functional**;
  only its shared chrome inherits the new style.
- Laundry and Camera share the same visual vocabulary without changes to the
  equipment notification controls or camera pixel pipeline.

**Firmware:** compile and flash ONLY Waveshare 4B v0.14.7. The already-working
controller Super Mini v0.7.1 does not need flashing. No NVS erase or
recalibration. Network, MQTT, HA, Wi-Fi/ESP-NOW failover, temperature logic,
and camera protocol are unchanged. Host code/layout checks are not a replacement
for a local ESP-IDF build and actual display acceptance.

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14
```

The example uses the most recently observed port, COM14. No GitHub Actions
build is requested. Check dial touch movement, whether the indicator is
visually aligned, all Home tile commands, Laundry/Camera, and the Ships tab.
If the panel shows a geometry or styling mismatch, use an actual photograph
for the next visual-only correction.


# Cottage Thermostat — v0.7.1 Controller / v0.14.5 Waveshare 4B acceptance

## v0.14.5 Design #3 native LVGL visual polish (no protocol change)

A native, memory-conscious UI-only theme refresh for the 480×480 Waveshare panel. This changes `touchscreen/main/room_display.c` styling: consistent midnight-blue palette, raised rounded cards, readable text and bright teal/electric-blue navigation accents, restrained amber heat-target colors. **Climate, Home, Laundry and Camera** receive matching visual styles. The **Ships** tab remains visible and functioning; its page contents are not updated in this release, only the shared app-shell header/nav. Camera is not placed on the Home page.

Climate keeps its saved target ring, 1-degree controls, Home/Away and Set Temp submission logic. Home keeps the same five compact tiles and Home Assistant actions. Laundry keeps four appliance states, activity indications and Notify buttons. Camera keeps exactly the same image decoding, viewport size, crop/fit, update strategy and connection handling. No modifications to controller firmware, ESP-NOW/Wi-Fi fallback, MQTT, HA devices or commands, sensors, mode persistence, calibration, motor safety, NVS or network timing.

**Local build only.** GitHub Actions is intentionally skipped. From ESP-IDF 6.0.1 PowerShell, repository root:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21
```

Verify the actual 4B screen and tap targets, including the Ships tab, before deployment. The local ESP-IDF compile and device rendering are still needed; static source and geometry checks are not hardware acceptance.



## v0.14.3 final 4B layout for local hardware testing

- Climate: the temperature ring is **252px**, centered and inset from the header to prevent the thumb clipping at the top; **-1 / Set Temperature / +1** occupy one bottom row, with **Away (50 F)** separate. Dragging the ring changes the proposed temperature without sending a command until Set.
- Home: five **224px square-ish tiles** in a two-column grid. **Deck Lights** and **Breezeway Lights** use a full-tile touch-to-toggle and illuminated bulb icon. **Blinds** has independent Open / Stop / Close buttons, **Vacuum** has Start / Stop, and **Kids Heat** has On / Off and the HA temperature reading. The bottom-right **sixth tile is intentionally unused** for a future control.
- Existing HA entity mapping, pending-command locks, known-state gating, ESP-NOW/servo separation, calibration and NVS storage are unaffected. One unavailable or stale light cannot be toggled blindly.
- Build on the user's Windows ESP-IDF 6.0.1 machine. **Do not trigger GitHub Actions**. Run `.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21` from the repo root after pulling. The controller **v0.7.1** is unchanged and does not need flashing.
- Host-only layout / light-toggle regression test: `test_room_home_layout` (pure C, no ESP-IDF). Check actual 4B touch responsiveness and element rendering after a local flash; CI/host tests cannot certify pixel rendering or the physical furnace.



**Release scope:** Persistent locally selected Service/Calibration vs Live controller operation; full-room SHT45 and AS5600 startup guards; quick temperature buttons; controller-owned 50°F Away / Freeze Hold on Waveshare and HA MQTT Climate preset. The servo controller continues accepting temperature and Away commands over the SAME leased/replay-protected Wi-Fi HTTP and encrypted ESP-NOW channels. Home Assistant never directly drives the servo or changes controller operating modes.

## Two distinct operating modes

- **Service / Calibration (default for existing installations and factory-reset NVS)** is persisted independently of Wi-Fi, pairing, and dial calibration. Boot keeps PWM disabled. Only the controller local webpage can enter attended 15-minute Test and move/jog/capture positions. The optional Bench Go session is still 15 minutes and is never resumed after reboot. Press **Enter Service / Calibration · Stop** at any time to stop output and persist Service, including across a restart.
- **Persistent Live** is explicitly selected on the controller webpage from **Service → Test**, after moving the physical dial to one captured numbered 1–6 position and selecting that number. It checks calibrated travel, all estimated temperatures, **all six saved AS5600 angles**, agreement of the *physical encoder* with the starting mark (within 12°), a valid primary SHT45 sample, and the target dial map. Then it saves the Live selection to NVS. Unlike Bench Go, it has no 15-minute session limit.
- **Live reboot:** GPIO5 is held low / PWM disabled first. The selected Live flag is read from NVS, but firmware does *not* trust the last commanded PWM. It waits for fresh primary SHT45 data and a fresh, magnet-valid AS5600 reading that maps uniquely to the saved 1–6 calibration. The reading must remain stable for 2.5 seconds across samples. Only then does it synchronize at the measured fractional dial position, arm Live, and command the calibrated target. If the sensor is missing, calibration invalid, magnet not credible, or position outside the saved 1–6 dial range, Live stays **blocked with PWM off**; the webpage explains why. There is no blind auto-homing.
- **After each Live movement**, new commands pause while encoder feedback is checked. If the expected saved encoder angle is not observed within 16 seconds, PWM fails, or the encoder is unreadable for 12 seconds, the controller stops and attempts to persist Service. This is a safety stop, not automatic retry or guaranteed physical burner shutdown. Physical testing is required: a stuck or externally moved dial may not be detected during every possible idle condition.
- **Service, Go and Live are deliberately not remotely selectable.** The touchscreen and Home Assistant can request temperatures and Away only.

## Climate controls

- Main 480×480 Climate page: **−5**, **−1**, **+1**, **+5** °F to edit the target, followed by **Set target** to submit once. This preserves the target bounds 45–80°F and reduces accidental rapid network changes.
- **Away / 50°F** on the same Climate page sets controller-owned, **NVS-persisted** Freeze Hold. The button changes to **Resume Home**. Normal target edits are blocked while Away is active. Exiting Away restores the pre-Away normal target; scheduled targets are bypassed during Away.
- MQTT-discovered **Thermostat** climate exposes preset modes **none** (Home) and **away**. It publishes both preset and heat-only target/current temperature state from the thermostat. The preset command `~/preset/set` accepts *only* `away` or `none`; it passes through the thermostat and paired controller link. It cannot arm PWM, set controller Service/Live, disable the furnace, or directly set a dial position. Climate continues without MQTT when Home Assistant is offline.
- **Freeze safety caveat:** 50°F is a temperature *request*, not a hard frost-protection guarantee. If the minimum calibrated dial setting corresponds to more than 50°F, Away commands dial **1** and reports `freeze_below_calibrated_range` instead of pretending the hardware can reach 50°F. If the minimum dial does not provide enough heat, real-world observations are needed. Use an independent low-temperature alarm/backup freeze-protection strategy for unoccupied winter use.

## v0.14.2 Waveshare-only UX/command update (local ESP-IDF build)

No controller firmware update is required after v0.7.1. The 4B screen now provides a **draggable target ring**, −1/+1 fine adjustment, a single **Set Temperature** confirmation, and a separate Away / Resume Home button. Moving the ring does **not** send a command. User-facing states replace raw internal error identifiers. Intentional controller mutations now do a fresh **read-only status handshake** before one authenticated, non-replayed request; a failure gives an actionable reason. The 50°F hold continues to be saved by the controller, not the screen or HA.

Home Assistant MQTT Climate reports **Home** or **Away** as named presets. HA adds a built-in **None** selection that cannot be renamed by device discovery; selecting None remains backward compatible with Home. The same thermostat device has an explicit Away / Freeze Hold switch for simple automation. After flashing the 4B, use its local settings → **MQTT thermostat** → **Republish HA discovery**. Test on the bench before enabling unattended Live control.

From the project root in ESP-IDF PowerShell:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21
```

Do **not** erase flash or NVS. Existing Super Mini v0.7.1 stays as-is. Confirm the new knob does not accidentally send commands during drag, the final **Set Temperature** sends exactly once, and repeated Home/Away taps report precise errors instead of generic rejection. Use `cmake -S tests -B build-host` and `cmake --build build-host`/`ctest --test-dir build-host` for optional local host tests, if you have CMake and a C compiler installed; adding `-DCJSON_SOURCE_DIR=...` enables MQTT-discovery tests.


## Field acceptance (must complete before unattended installation)

1. With servo **unmounted and unloaded**, flash controller v0.7.1 and Waveshare v0.14.1 using the local private SHT45 profile. Do **not** erase NVS. Verify controller online + encrypted ESP-NOW fallback after a temporary thermostat-only Wi-Fi block.
2. In persistent Service, use Test to move all captured positions in both directions; verify **GPIO5 repeated moves** and live AS5600 angles follow. Ensure STOP disables PWM and persists Service over reset.
3. Confirm ±1/±5 buttons change the proposed temperature without repeated tiny taps; submit **Set target** once. Verify controller receives it and moves appropriately in Bench Go. Check the touch buttons don't overlap the 4B bottom navigation.
4. Confirm Away on the 4B reports 50°F. In Home Assistant, Climate → Preset → Away also sets 50°F; Preset → None or **Resume Home** restores the previously requested comfort target. Test while MQTT is down; ESP-NOW should still carry the paired command.
5. Only after installing a secure mount and performing **final calibration on the real dial**, capture the numbered positions and AS5600 angles again. Move in Service Test to a known numbered mark, verify the encoder matches, then click **Enable persistent Live**. A restart should begin PWM-off and only then recover automatically from a fresh sensor and encoder reading.
6. Test controlled **power-cycle recovery**, Wi-Fi loss/fallback, Home Assistant loss, SHT45 disconnect, invalid/shifted magnet, encoder angle mismatch, stopped/stalled servo, stuck pulse command, and emergency ability to operate the original Williams heater manually. Failure must not blindly move the actuator. Check log and `/api/status` for `operating_mode`, `live_active`, `live_issue`, `away`, and `freeze_below_calibrated_range`.
7. Observe multiple **full actual furnace heating/cool-down cycles** with primary room temperature and furnace power. Validate 2°F/5-minute overshoot trim and learned dial estimates. No calibrated heating-cycle evidence means no claim of reliable frost protection or unattended readiness.

## Local flash — distinct targets, no NVS erase

From the ESP-IDF 6.0.1 PowerShell environment and repo root, after `git pull --ff-only`:

Controller Super Mini, normally COM17:
```powershell
.\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM17
```

Waveshare 4B touchscreen, normally COM21, retaining local SHT45 sensor configuration:
```powershell
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21
```

Build script uses `touchscreen/sdkconfig.4b`, 16MB flash + octal PSRAM, and local secrets. Keep the monitored COM ports separate. Confirm local configurations retained the SHT45 selection. Upgrading only one board may leave the new Away operation unavailable; update both versions together.

**Important:** Passing CI tests and ESP-IDF compilation is necessary but not sufficient for gas furnace unattended deployment. The new Live behavior is intentionally guarded and should remain in persistent Service until the physical system has passed the above acceptance tests.

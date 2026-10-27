# Cottage Thermostat — v0.7.0 Controller / v0.14.0 Waveshare 4B acceptance

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

## Field acceptance (must complete before unattended installation)

1. With servo **unmounted and unloaded**, flash controller v0.7.0 and Waveshare v0.14.0 using the local private SHT45 profile. Do **not** erase NVS. Verify controller online + encrypted ESP-NOW fallback after a temporary thermostat-only Wi-Fi block.
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

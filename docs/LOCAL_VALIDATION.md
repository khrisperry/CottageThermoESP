# Local validation and next cottage visit

Firmware milestone: **v0.2.0**, 2026-10-18. Build from a configured **ESP-IDF 6.0.1** terminal; user machine is Windows/VSCode. The checked-in PowerShell helper builds both firmware projects, does not flash, and checks exit codes.

## Before next weekend

1. Pull/check out the implementation branch described in the delivery message. Do not discard local changes.
2. From repository root:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1
   ```

3. For the AtomS3 Lite only, build/flash from `controller` on its confirmed port (last log: COM13). Default firmware starts automatic control in **simulation**, PWM off.
4. Pair the controller web page with the USB-monitor token. Verify `simulation:true`, `pwm_active:false`, `position_measured:false`. Review modes and persisted settings. Reboot and verify PWM remains off and session synchronization/arming are cleared.
5. For pure software testing, create a synthetic map in the simulation build, synchronize a virtual level, submit a simulation reading and select Thermostat. Verify 0.25-level bounded simulated commands, dwell, target outside map, stale-source and Hold behavior. Restore Hold when finished.
6. Set the clock from the UI in AP-only mode; configure a weekly schedule. Before the first day's slot, the previous day's last slot applies. A manually saved target overrides it. After reboot without a valid clock, scheduled movement is blocked.
7. Build `touchscreen` now. Set the controller token/network in its menuconfig. Leave sensor disabled and pins -1 until actual wiring is known; the project sends no made-up temperature. Probe/LCD hardware need not be attached for compilation.

## At the cottage / physical proof

- Keep automatic physical thermostat output disabled for the initial motor/coupling test.
- Unloaded servo, external supply with common ground, G38 signal. Hold → arm → explicit 1500 µs → small pulse changes. Record movement, current, repeatability and usable endpoints. Stop PWM and power-cycle at several positions, observing movement.
- Hub coupon **102** was reported as the perfect shaft fit. This is a fit observation, not proof of torque, available travel, coupling retention or encoder alignment.
- Measure actual knob arc/limits, dial-to-pulse references and temperatures; do not assume estimated 120° travel or nominal 270° servo travel.
- Physical auto bench mode is a separate compile option; it uses a separate calibration store and additionally requires explicit session arm plus observed-position sync. A first PWM command may still jump. No encoder/stall/coupling validation is present yet.
- Never infer the physical shaft position from the displayed command. Once a suitable encoder arrangement is selected, add its adapter and movement verification before permanent control.

## Repeatable developer checks

Host engine checks (native C toolchain):

```sh
cmake -S tests -B tests/build -G Ninja
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

This exercises actual shared C code: startup interlock, missing/invalid/reversed calibration, no extrapolation/level 0, primary/secondary freshness, disagreement, out-of-order samples, modes, step/dwell limits, weekly boundaries, unavailable time and a longer simulated run.

Optional UI checks (Node + Playwright):

```sh
cd tests
npm install
npx playwright install chromium
npm test
```

These use mocked HTTP responses to check mobile layout, settings, pairing failure, Stop/schedule actions, room display and connection loss. They do **not** establish live ESP HTTP/network/NVS behavior.

## Explicit remaining modules

- Physical LCD/touch/LVGL driver and exact board/probe pin assignment.
- Encoder mounting/driver, actual position agreement, slip/stall/coupler monitoring and verified mechanical limits.
- Wi-Fi provisioning page/captive DNS; SSID/password currently use menuconfig.
- Native MQTT Discovery/climate integration and actual HA entity mapping; SwitchBot reports can already enter through the paired API.
- OTA image validation/rollback and production transport/security.
- Durable long-term observations, recovery model, weather/activity/occupancy inputs, heading-to-cottage ETA automation and active learning. Active exploration is **not implemented** and cannot start accidentally.

These are development firmware builds, not validated appliance automation. Hardware/runtime checks above remain separate from compiler and host/UI test evidence.

## Build/test evidence for this milestone

- ESP-IDF tag **v6.0.1**, Xtensa GCC **15.2.0 (esp-15.2.0_20251204)**, ESP32-S3, 8 MB flash defaults.
- Controller and room-client ELF/binary generation and app-partition size checks passed on the development host. These are compile/link results, not physical board tests or a build executed on Khris's Windows PC.
- Native C engine checks passed, including address/undefined-behavior sanitizer execution (leak scanning disabled for the host sandbox).
- Browser smoke tests passed using actual Chromium with mocked API responses. No live ESP HTTP, NVS recovery or probe hardware test has been performed here.

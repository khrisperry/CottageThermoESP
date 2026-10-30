# V1.0.X production build and upgrade guide

## Version and source

`main` carries the consolidated production baseline. `VERSION` contains `1.0.0`; both applications use this value in their ESP-IDF metadata. Their existing application names remain unchanged so OTA identity checks keep working. Release tags use `V1.0.0`, `V1.0.1`, etc.

The initial baseline includes controller 0.7.5 and room thermostat 0.15.4 behavior. It does not change the calibration structure, NVS keys, radio protocol, Wi-Fi power, servo pins, partition offsets or thermostat movement policy.

## Update the local checkout

Commit any intended source edits first; leave ignored local secrets/configuration in place. After the production branch is integrated:

```powershell
git fetch origin
git switch main
git pull --ff-only
```

If an earlier cherry-pick is on your feature branch, switching to main avoids replaying it. Keep the feature branch until you have confirmed your local changes are accounted for. No hard reset, clean-all or erase-flash step is needed.

## Build prerequisites

Use ESP-IDF **6.0.1** with its ESP32-S3 toolchain from the VSCode ESP-IDF PowerShell terminal. The root build helper chooses the actual production boards. The controller uses `controller/sdkconfig.supermini` and `build-supermini`; the thermostat uses `touchscreen/sdkconfig.4b` and `build-4b`.

Both local SDK configs and `touchscreen/secrets.h` are retained and ignored by Git. On first build the 4B helper creates `secrets.h` from the example. Fresh room configs select SHT45/SHT4x on GPIO47/48. To deliberately migrate an existing local room config to SHT45, run the original 4B helper with `-Sht45`. Do not use the BME280 test profile for primary furnace control; its readings are display-only.

## Build / USB upgrade

From the repository root:

```powershell
.\tools\build.ps1
.\tools\build.ps1 -Device controller -Flash -Monitor -Port COM17
.\tools\build.ps1 -Device thermostat -Flash -Monitor -Port COM14
```

Replace COM ports with those currently assigned to each board. Single-dash switches are required; unknown arguments now fail instead of silently producing a build-only run. Flash/Monitor require one selected device and an explicit port. The root helper does not flash when called without `-Flash`.

Existing helpers remain valid:

```powershell
.\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM17
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM14
```

Normal flashing preserves saved Live/Service selection, calibration, target, Wi-Fi, pairing and temperature offset. Confirm firmware version **1.0.0** in the boot log/device status, then hard-refresh the webpages. The controller remains stopped during boot until its Live prerequisites validate.

## Calibration and Live

Choose Service/Calibration, then Test Mode for manual movement. Move the physical knob to each numbered marking and click Capture beside it to replace the saved pulse and AS5600 angle. Estimated temperatures are a separate mapping. The always-visible Calibration & Live checks table shows Off/1–6/Max pulse, angle and estimated temperature, plus measured position and commanded expected-angle error.

Go Live saves persistent intent immediately. The control loop recovers the actual fractional dial position after stable encoder/calibration and fresh primary temperature checks. It does not require Test PWM or an exact selected integer marking. Waiting prerequisites are displayed. Each servo movement remains checked against the saved angle map. Bench Go is a separate attended 15-minute session and does not persist.

## Device names and updates

Current deployed names are retained: thermostat DHCP hostname `CottageThermostat`; controller DHCP/mDNS `CottageThermoController` / `CottageThermoController.local`. Retaining the controller name preserves room-panel discovery.

The controller uses USB updates. The thermostat supports application OTA from its webpage when the existing OTA partition layout and rollback bootloader are installed. Preserve `cottage_thermo_room_4b` identity; upload its application `.bin`, not a controller image, bootloader, partition file or ZIP. First-time partition/bootloader installation uses USB.

## Release packages

After local builds:

```powershell
python .\tools\package-release.py
```

This produces two ZIPs in `dist/`, one per device. Each contains only the expected app/bootloader/partition/flash files plus a manifest, SHA256SUMS and flashing instructions. Packaging reads the binary's embedded version and application identity and rejects stale/wrong images. It does not include SDK configs, secret headers, ELF files or build logs. Locally compiled application binaries may themselves contain private credentials; keep those packages private.

CI builds start with empty example credentials. A new `VERSION` merged to main runs tests, builds both applications, then creates the matching `V1.0.X` tag/release with both archives after success. Existing releases are left intact. Explicit tag builds also verify that the tag matches `VERSION`. Do not reuse a release tag; advance the patch number for later changes.

## Validation

Host checks cover thermostat policy, calibration HTTP workflow, encoder recovery, PWM updates, connection recovery, freshness, furnace stages, weather and release packaging. Embedded web tests cover both device interfaces. ESP-IDF firmware builds validate the actual hardware targets. The working baseline was reported functional on the bench; final installed furnace validation remains part of installation.

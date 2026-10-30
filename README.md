# CottageThermoESP — V1.0.X

Coordinated firmware for the cottage furnace dial controller and room thermostat. The first release is **V1.0.0**. Both devices read the same root `VERSION`; subsequent fixes use V1.0.1, V1.0.2, and so on.

| Device | Production hardware | Build helper | Firmware identity |
|---|---|---|---|
| Controller | HW-747 ESP32-S3 Super Mini, 4 MB | `tools/build-controller-supermini.ps1` | `cottage_thermo_controller` |
| Thermostat | Waveshare ESP32-S3 Touch-LCD-4B, 16 MB | `tools/build-room-4b.ps1` | `cottage_thermo_room_4b` |

Use [the production guide](docs/PRODUCTION.md) for builds, flashing, settings, calibration and release packaging. See [V1.0.0 release notes](docs/releases/V1.0.0.md) and [CHANGELOG.md](CHANGELOG.md).

## Build and flash

Run from the repository root in an **ESP-IDF 6.0.1 PowerShell terminal**. Build both without flashing:

```powershell
.\tools\build.ps1
```

Flash one device at a time, using its actual COM port:

```powershell
.\tools\build.ps1 -Device controller -Flash -Monitor -Port COM17
.\tools\build.ps1 -Device thermostat -Flash -Monitor -Port COM14
```

PowerShell switches use one dash: `-Flash`, `-Monitor`. The existing board-specific helpers still work; `build-local.ps1` now selects the production Super Mini and 4B profiles. Local configs/secrets are ignored by Git and retained. Normal upgrades do not require erasing NVS or recapturing calibration.

## Included behavior

- Persistent Live, Service/Calibration, attended Test and 15-minute Bench Go.
- Measured fractional dial recovery, encoder checks on startup and movement, and a visible calibration/expected-angle table.
- LAN/mDNS discovery, paired encrypted ESP-NOW fallback and controller link recovery.
- SHT45 primary temperature, saved sensor offset, standalone thermostat and optional HA/MQTT Climate integration.
- Native Climate/Home/Laundry/Camera/Ships pages, digital clock screensaver, HA furnace power stages and outdoor weather.
- Thermostat webpage settings, restart and rollback-capable application OTA; controller USB updates.

The release preserves the tested control logic. Furnace power and weather are observations; adaptive learning and automatic thermal trim remain disabled. Bench verification and final furnace installation validation are separate.

## Repository

| Directory | Purpose |
|---|---|
| `controller/` | Production Super Mini firmware and calibration webpage |
| `touchscreen/` | Production Waveshare 4B thermostat |
| `shared/` | Thermostat engine and paired transport |
| `tools/` | Build helpers, release packager and HA mapping generator |
| `tests/` | Host, embedded web workflow, native LVGL and packaging checks |
| `hardware/` | Mechanical/CAD work |
| `docs/` | Production guide and engineering references |
| `docs/history/` | Earlier development logs |
| `ha_bridge/` | Optional legacy bridge; direct HA does not require it |

`main` is the consolidated production source. Release tags are `V1.0.X`. Development branches remain available for history; they are not the upgrade path. Application identities, partition offsets and saved settings formats are retained from the working baseline.

## Validation and artifacts

The production workflow tests the host logic/web interfaces, builds both devices with ESP-IDF 6.0.1, and packages each binary with its bootloader, partition table, flash arguments, manifest and SHA-256 checksums. A new VERSION on main publishes its matching V1.0.X release only after all jobs succeed; existing releases are left intact. CI builds contain example/empty credentials; configure integrations through the device pages. A firmware binary built locally may contain your compiled credentials.

For local release archives after building:

```powershell
python .\tools\package-release.py
```

Archives are written to ignored `dist/`. The packager rejects a binary whose embedded version or application identity does not match the release/device.

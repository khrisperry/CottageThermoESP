# Controller v0.5.3 — thermostat development and servo bench

ESP-IDF **6.0.1**, ESP32-S3, M5Stack AtomS3 Lite (8 MB flash), servo signal **G38/GPIO38**. Build defaults simulate automatic thermostat movement. No measurements or placeholder calibration map are installed.

v0.4.1 keeps thermostat/servo decisions on this standalone controller and removes its MQTT client and command handlers. The room panel supplies local temperature reports and setpoint requests over the paired HTTP/ESP-NOW link. Home Assistant can request a target temperature through display v0.12.1's Climate entity; the display uses its normal validated controller link. HA has no direct controller/MQTT mode or servo commands. Neither HA nor a broker is required for the thermostat engine. See [standalone MQTT monitoring](../docs/STANDALONE_MQTT_MONITORING.md).

Paired radio configuration from the display webpage and the 2 MiB app partition remain. Configuration changes restart in Hold with bench output released. Controller firmware updates still use USB. Bench output interlocks, simulation default and explicit arming/synchronization are retained.

See [controller Wi-Fi setup and Omada/ESP-NOW testing](../docs/CONTROLLER_LINK_TESTING.md) for the current connection workflow. Wi-Fi credentials can now be saved on the controller webpage; menuconfig values are the initial defaults.

## Build on Windows

In the VSCode ESP-IDF PowerShell terminal, from your existing repository:

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\build-local.ps1
```

This builds both projects without flashing. To build just the controller:

```powershell
.\tools\build-local.ps1 -Project controller
cd controller
idf.py -p COM13 flash monitor
```

COM13 was the controller port in the latest supplied build log; confirm it still belongs to the AtomS3 Lite before flashing. For a fresh checkout run `idf.py set-target esp32s3` first. An existing correct `sdkconfig` is retained; new options take their defaults. Do not erase flash/NVS to update firmware. IDF downloads the pinned `espressif/cjson` component during the first build; IDF 6 removed the built-in JSON component.

## First use

1. Connect to **CottageThermoBench**, open Wi-Fi, no password. Open **http://192.168.4.1** manually (DNS captive-portal interception is not implemented).
2. The webpage opens directly. Local setup and the controller API require no access token.
3. The first boot is **Hold**, no map, unknown physical position, PWM off. Review status and settings without enabling motion.
4. For software testing, enter a synthetic map **only in the simulation build**, synchronize a virtual starting dial level, submit a simulation temperature, then select Thermostat. Simulated movement is visible in events. One temperature report expires after 90 seconds.
5. For the actual servo test: remove the horn/load, use Hold mode, **Arm unloaded bench output**, then send **1500 µs** followed by small manual pulse changes. Raw bench commands accept the full **500–2500 µs** range, including both endpoints. Find the actual usable travel with the servo unloaded; pulse width does not verify angle or position. The measured thermostat calibration map still uses 1100–1900 µs. **Raw PWM is physical even in a simulation build.** It is separately armed; the normal thermostat engine remains simulated. Stop PWM clears synchronization and disables the signal. No return to zero.

No synthetic map is shipped. **Simulation and live bench maps/settings use different NVS keys**, so a simulated map cannot silently become a live calibration when you change the build option. Session arming and synchronization never persist. Neither an NVS record nor a pulse command is proof of physical position.

## Implemented behavior

- Persistent target, mode, map, schedule, source preference and movement limits in a schema-validated NVS record. Writes happen only on changed settings, not per sample or movement. Invalid old records fall back to Hold with an empty map; they are not erased automatically.
- Hold leaves the current mechanical setting; Manual disables PWM and invalidates synchronization. Direct dial mode accepts fractional levels only within the measured map. No normal command supports level 0.
- Thermostat maps desired °F to empirically measured Williams dial levels. The Williams thermostat performs heat cycling. Room-temperature error is **not** used to repeatedly wind the dial higher/lower.
- Thermostat moves require a valid map, fresh temperature and session synchronization. Direct dial requests require a valid map and session synchronization. Default maximum step **0.25 dial level**, minimum dwell **5 minutes**, level deadband **0.1**. No extrapolation beyond measured points. Both servo directions are supported.
- Primary **room** probe report, optional **switchbot** secondary. Valid range 32–110°F, TTL **90 seconds**. Secondary disabled by default. If both enabled sources are fresh and disagree by more than **5°F**, thermostat movement pauses. Unsupported sources (including Kids/Keg) are rejected.
- A weekly schedule with up to **14** entries; Detroit local time with DST. Latest entry carries across days/week boundaries. Unknown time blocks scheduled motion. Entering a manual target disables the schedule; explicitly re-enable it to resume.
- Optional station Wi-Fi (saved on the controller webpage, with menuconfig defaults); AP remains available. LAN mode uses NTP; AP-only boot requires the UI's **Set clock from this device** for scheduling.
- Last 64 events in RAM, one-minute serial samples, six-hour CSV sample ring in RAM. History clears on reboot. Unix timestamp 0 means clock unavailable; `nan` means no valid reading/command. No passive data is represented as a trained model or measured servo angle.

## Wiring

| AtomS3 Lite / supply | Servo |
|---|---|
| G38 / GPIO38 | Signal (3.3 V logic) |
| GND | Servo ground and external supply negative |
| Separate regulated 5 V supply, sized for actual stall/start current | Servo positive |
| USB-C | AtomS3 power, flash, monitor |

Do not supply motor current from a GPIO/3.3 V pin. Stop PWM is not a brake or guaranteed free rotation; test actual servo behavior.

## Later physical thermostat bench mode

`idf.py menuconfig` → Cottage thermostat → **Enable physical output from thermostat engine**. Default OFF. Rebuild/flash, enter the measured bench map, explicitly arm and synchronize the observed current position. This enables bounded **attended inert-fixture** movement; it still has no encoder, stall detection or verified appliance coupling. Arming expires after **15 minutes**, disables PWM and clears synchronization. Reboot starts PWM off regardless of saved mode/target.

Hardware-dependent gates remain actual travel/endpoints/torque, coupling, independent angle feedback, board wiring and installation validation. Controller OTA/rollback, active learning and weather/occupancy context remain future work. MQTT monitoring belongs to the wall display.

For the exact API see [../docs/API.md](../docs/API.md); for pre-visit steps see [../docs/LOCAL_VALIDATION.md](../docs/LOCAL_VALIDATION.md).

## AS5600 read-only bench test (v0.5.4)

Wire VCC to 3.3 V, GND to ground, SDA to G6 (GPIO6), SCL to G7 (GPIO7).
OUT/GPO/DIR are unused by this I²C test. The default I²C address is 0x36,
polled twice per second at 100 kHz. Pins can be changed in menuconfig.
Open the controller webpage's **AS5600 magnetic encoder** section. With a
centered magnet, expect **Magnet detected · Field OK** and angle changes
when rotating by hand. The webpage refreshes every three seconds; serial logs
report every five seconds and on I²C availability changes. The angle wraps at
360 degrees. Missing/weak/strong magnets suppress the usable angle. Failed or
stale reads never retain a valid angle. Reconnecting the sensor recovers polling.
No arming is required, and this feature never moves the servo, writes encoder
configuration, or verifies the calibrated furnace dial position.

Register reference: https://look.ams-osram.com/m/7059eac7531a86fd/original/AS5600-DS000365.pdf

## HW-747 Super Mini 4 MB bench firmware (v0.5.5)

For the photographed ESP32-S3FH4R2 board (4 MB flash, 2 MB QSPI PSRAM),
use the dedicated profile. PSRAM is not needed and stays disabled. The factory
application partition remains 2 MB; the partition table ends at 0x210000, within
4 MB. This build supports USB flashing; it does not add OTA partitions.

From the repository root in an ESP-IDF 6.0.1 PowerShell terminal:

```powershell
.\tools\build-controller-supermini.ps1
# After confirming the new board's port in Device Manager:
.\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM13
```

Replace COM13 with the new board's actual port. The script uses
`controller/sdkconfig.supermini` and `controller/build-supermini`, leaving the
existing controller configuration alone. Future rebuilds must use this script
(or the same SDKCONFIG and build directory). Do not flash the ordinary
`controller/build` artifacts to this 4 MB board.

| Connection | Super Mini pad |
|---|---|
| AS5600 VCC | 3V3 |
| AS5600 GND | GND |
| AS5600 SDA | GPIO6 |
| AS5600 SCL | GPIO7 |
| Servo signal | Inner GPIO38 pad |
| Servo ground | GND (shared with its power supply) |
| Board power | USB-C, 5 V |

Use a suitable regulated 5 V supply for the servo. The board's 5V pad can only
be used for servo power if its USB power path and supply support the combined
load. B+/B- remain unused for these USB bench tests. The servo's supply is never
3V3. Solder all connections and inspect for bridges, especially at GPIO38.

Automatic thermostat output remains simulated. Raw 500-2500 us PWM commands
still require Hold and explicit bench arming. Boot leaves PWM off. Encoder
telemetry remains read-only and does not establish furnace dial position.
Startup prints the board profile and GPIOs; `/api/status` includes the board
name and servo GPIO. Open the encoder section to test by hand before arming.

The replacement board has fresh NVS and a different MAC. Configure its Wi-Fi
through the CottageThermoBench access point (192.168.4.1), then set its LAN URL
on the wall thermostat. Update the ESP-NOW controller peer MAC and synchronize
pairing through the thermostat. Settings stored on the original board do not
transfer automatically. The wall thermostat does not need reflashing.

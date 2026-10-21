# Controller v0.4.1 — thermostat development and servo bench

ESP-IDF **6.0.1**, ESP32-S3, M5Stack AtomS3 Lite (8 MB flash), servo signal **G38/GPIO38**. Build defaults simulate automatic thermostat movement. No measurements or placeholder calibration map are installed.

v0.4.1 keeps thermostat/servo decisions on this standalone controller and removes its MQTT client and command handlers. The room panel supplies local temperature reports and setpoint requests over the paired HTTP/ESP-NOW link. Home Assistant only monitors the display's read-only MQTT sensors; it cannot change controller setpoint, mode or servo output. Neither HA nor a broker is required for the thermostat engine. See [standalone MQTT monitoring](../docs/STANDALONE_MQTT_MONITORING.md).

Paired radio configuration from the display webpage and the 2 MiB app partition remain. Configuration changes restart in Hold with bench output released. Controller firmware updates still use USB. Bench output interlocks, simulation default and explicit arming/synchronization are retained.

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
2. Copy the **Pairing API token** printed in the USB monitor into the page. Token is generated once and stored in NVS. The open Wi-Fi remains password-free; API access is paired separately.
3. The first boot is **Hold**, no map, unknown physical position, PWM off. Review status and settings without enabling motion.
4. For software testing, enter a synthetic map **only in the simulation build**, synchronize a virtual starting dial level, submit a simulation temperature, then select Thermostat. Simulated movement is visible in events. One temperature report expires after 90 seconds.
5. For the actual servo test: remove the horn/load, use Hold mode, **Arm unloaded bench output**, then send **1500 µs** followed by small manual pulse changes. **Raw PWM is physical even in a simulation build.** It is separately armed; the normal thermostat engine remains simulated. Stop PWM clears synchronization and disables the signal. No return to zero.

No synthetic map is shipped. **Simulation and live bench maps/settings use different NVS keys**, so a simulated map cannot silently become a live calibration when you change the build option. Session arming and synchronization never persist. Neither an NVS record nor a pulse command is proof of physical position.

## Implemented behavior

- Persistent target, mode, map, schedule, source preference and movement limits in a schema-validated NVS record. Writes happen only on changed settings, not per sample or movement. Invalid old records fall back to Hold with an empty map; they are not erased automatically.
- Hold leaves the current mechanical setting; Manual disables PWM and invalidates synchronization. Direct dial mode accepts fractional levels only within the measured map. No normal command supports level 0.
- Thermostat maps desired °F to empirically measured Williams dial levels. The Williams thermostat performs heat cycling. Room-temperature error is **not** used to repeatedly wind the dial higher/lower.
- Thermostat moves require a valid map, fresh temperature and session synchronization. Direct dial requests require a valid map and session synchronization. Default maximum step **0.25 dial level**, minimum dwell **5 minutes**, level deadband **0.1**. No extrapolation beyond measured points. Both servo directions are supported.
- Primary **room** probe report, optional **switchbot** secondary. Valid range 32–110°F, TTL **90 seconds**. Secondary disabled by default. If both enabled sources are fresh and disagree by more than **5°F**, thermostat movement pauses. Unsupported sources (including Kids/Keg) are rejected.
- A weekly schedule with up to **14** entries; Detroit local time with DST. Latest entry carries across days/week boundaries. Unknown time blocks scheduled motion. Entering a manual target disables the schedule; explicitly re-enable it to resume.
- Optional station Wi-Fi (`idf.py menuconfig` → Cottage thermostat); AP remains available. LAN mode uses NTP; AP-only boot requires the UI's **Set clock from this device** for scheduling.
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

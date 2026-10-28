# Controller v0.7.2 — blue status LED on HW-747 Super Mini

The Super Mini's **onboard blue RGB LED** is used for connection and fault
status, leaving the board's red power/charging indicator alone. This
applies only to the `CONFIG_THERMO_SUPERMINI` build; no LED pins are taken
from the servo GPIO5 or AS5600 GPIO6/GPIO7.

| Blue LED | Meaning |
|---|---|
| **Off** | The Waveshare is currently communicating with the controller through verified LAN/Wi-Fi HTTP; all normal link/fault checks pass |
| **Flashing (0.5 s on / 0.5 s off)** | ESP-NOW is the freshest paired link (even if the controller still has a Wi-Fi address) |
| **Solid** | Persistent Live fault/PWM fault, or no recent authenticated room link after a one-minute boot grace period |

**Service/Calibration is NOT itself a fault.** A normal Service page may be
healthy with the blue LED off when the Waveshare has been communicating via
LAN. A Live failure always overrides an otherwise healthy connection.
These LEDs are **indicators only**; they do not arm, move, release or
otherwise control the furnace. The brightness is intentionally low (blue
channel 18/255) for nighttime use. The LED runs in a separate low-priority
task using a single WS2812 RGB pixel on **GPIO48**, as commonly wired on the
HW-747 ESP32-S3 Super Mini. Generic S3 SuperMini board revisions differ
(e.g., some share GPIO48 with other LED circuits). **Verify the blue LED
on the exact physical controller at the bench**, especially that the red
power light behaves normally. The firmware cannot switch off a hardwired
red power/charging LED.

After boot, allow up to 60 seconds for Wi-Fi association and ESP-NOW pairing
before treating solid blue as a communication fault. The room link expires
after 20 seconds without recent controller traffic. The controller's
`/api/status` `network.blue_led` field, and the controller webpage
**Wi-Fi & connection** panel, report the chosen LED status. Note that an
ESP-NOW reply queued successfully by the controller is not definitive proof
the remote Waveshare received it; diagnose persistent blue with the
Waveshare's own transport page and serial logs.

**Local build only**; no GitHub Actions. In ESP-IDF 6.0.1 PowerShell, from
the repository root:

```powershell
git pull --ff-only
.\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM13
```

Replace COM13 with the actual controller's USB port. Do not erase NVS.
**Only the Super Mini needs flashing** for the LED behavior; the Waveshare
does not. Existing firmware pairing, Wi-Fi 13 dBm power setting, MQTT,
sensor readings and servo commands are unchanged. The new host-only
`test_controller_status_led` validates all three states and priority
without RMT/ESP-IDF hardware.

## Previous controller v0.7.1 — guarded persistent Live, Service and 50°F Freeze Hold

**New persistent modes:** Service/Calibration is the safe default (PWM off on boot). After final mounted calibration, select Test, move to a saved numbered dial position, verify the AS5600, then select **Enable persistent Live**. Controller NVS remembers Live through firmware flashing and power loss, but never moves until startup validates the full dial/angle map, a stable magnetic position, and a fresh primary SHT45 report. Live has no 15-minute bench timeout; every Live movement requires AS5600 follow-up or it stops and attempts to persist Service. **Service** remains the only way to manually calibrate/test. The old 15-minute Go remains available for supervised bench use. Home Assistant cannot arm Live.

**Freeze Hold / Away:** The room panel and existing Home Assistant MQTT Climate preset modes **Away / None** use the same paired transport and controller-owned NVS flag. Away targets **50°F** and preserves the normal target. Home restores the normal target. This is heat-only; no AC or off command. If dial 1 maps warmer than 50°F, Away commands dial 1 and reports the limitation; verified frost protection requires actual cottage observations and a backup warning mechanism. Touchscreen Climate now offers **−5, −1, +1, +5**, Set target, and Away / Resume Home.

**Companion Waveshare:** v0.14.0. Flash **both** ESP32 boards without erasing NVS to use the new paired Away operation. Full test matrix, precautions, known limitations and commands: [Installation / production-readiness acceptance](../docs/PRODUCTION_READINESS_V070.md).

## Previous v0.6.5 fix

# Controller v0.6.5 — restore repeat servo movement (ESP-IDF v6.0.1)

**Critical v0.6.5 fix:** v0.6.4 configured servo PWM on GPIO5 successfully on the first command, but its subsequent `ledc_set_duty_and_update()` call failed because that ESP-IDF v6.0.1 API requires the LEDC fade service even though the servo has no fade animation. The failure deliberately disarmed Test/Go, making the controller appear stuck after the first pulse. v0.6.5 routes subsequent commands through a tiny, separately host-tested non-fade `ledc_set_duty()` + `ledc_update_duty()` driver, which also re-enables the channel after `ledc_stop()`. We now regression-test repeated commands, stop/restart, and failures with the actual production implementation. The controller retains the 13 dBm Wi-Fi limit, mDNS hostname, current dial/temperature calibration, ESP-NOW pairing and 15-minute attended Go safety gates. **Flashing the new Super Mini image does not require erasing NVS or reflashing the Waveshare thermostat.** Bench-test repeated manual pulses and 50→65→70°F Go setpoint changes unloaded before mounting.

## Previous v0.6.4 feedback behavior

# Controller v0.6.4 — responsive setpoints plus heat-only feedback

**v0.6.4 provisional heat feedback:** Changing the thermostat setpoint still moves promptly to the requested calibrated dial (after a maximum 2-second command guard). Then the firmware watches fresh primary room temperatures; it records the observed peak/reached target and requires a **2°F overshoot sustained for 5 minutes**, plus at least **10 minutes since the setpoint**, before **lowering** the dial at most **0.25 setting**. Additional downward corrections are at least **15 minutes apart**, with at most **one dial setting total reduction** and no adjustment below dial 1. Any new target resets the transient trim. This prevents temperature noise and short residual-heat spikes from changing the knob repeatedly. The adaptive trim and peak are **runtime observations only**: they do NOT automatically change the saved furnace-number temperature estimates, and no single heating cycle is treated as proven calibration. There is no cooling/AC or automatic increase algorithm yet.

**Safety/testing limitation:** In the current 15-minute **attended Go bench mode**, meaningful whole-cottage heating and repeated-cycle learning cannot be validated; the device still disarms and stops PWM when time expires, after restart, or on Stop. Continue with controlled fixture tests and separately record room temperature / furnace power through full cycles before enabling any unattended feedback mode. The AS5600 remains telemetry-only. Local Wifi and ESP-NOW pairing, dial limits, and temperature estimates persist across flashing.

## Previous v0.6.3 changes

# Controller v0.6.3 — responsive setpoints and safer PWM channel usage

**v0.6.3 prompt setpoints:** In attended **Go** mode, thermostat/panel/Home Assistant/scheduled target updates move directly to the calibrated dial value after at most a short 2-second anti-chatter guard. The five-minute / quarter-dial policy now applies only to non-setpoint adjustments. Room temperature alone does not drive oscillatory dial corrections: the target is mapped to the Williams dial's empirical temperature setting. All calibration/travel, position-sync, fresh sensor, valid target, explicit Go, Stop, and 15-minute bench expiry checks remain. Go still does **not** restore after restart, and this is **not** unattended furnace firmware. Servo PWM now configures LEDC channel once and changes duty for subsequent commands rather than attempting to reserve GPIO5 repeatedly; verify the pin and AS5600 motion in the unloaded fixture before installation.

## Previous v0.6.2 changes

**v0.6.2 — editable travel limits, guided dial setup and bench control**

**v0.6.2 endpoint correction:** On the controller webpage, Calibration → **Re-measure start limit…** or **Re-measure end limit…** stops PWM, exits Go/Test, clears only the selected limit and retains the other limit, all eight dial positions/recorded angles and all six estimated temperatures. Re-enter attended Test Mode, move carefully using a starting pulse and small jogs, then **Capture current start/end** to save the corrected boundary. With either bound missing, manual PWM is restricted only to the 500–2500 µs firmware limits and **Go is blocked**. Do not perform this on the mounted furnace; use an unloaded supervised bench. Correct any captured dial position outside a revised narrower range before completing travel limits. Calibration stays in NVS; never erase flash/NVS to correct an endpoint.


v0.6.1 restores the Super Mini 13 dBm TX limit after authentication timeouts at the 20 dBm default. Calibration/Test/Go are unchanged. See [Wi-Fi comparison notes](../docs/CONTROLLER_WIFI_V061.md).

Use [the v0.6.0 workflow](../docs/CONTROLLER_WORKFLOW_V060.md) for the current webpage, calibration, Test and physical Go controls. The instructions below describe the earlier v0.5.x interface and are retained as hardware/API history; the old compile-time live-bench switch is retired.


ESP-IDF **6.0.1**, ESP32-S3, M5Stack AtomS3 Lite (8 MB flash), servo signal **G38/GPIO38**. Build defaults simulate automatic thermostat movement. No measurements or placeholder calibration map are installed.

v0.4.1 keeps thermostat/servo decisions on this standalone controller and removes its MQTT client and command handlers. The room panel supplies local temperature reports and setpoint requests over the paired HTTP/ESP-NOW link. Home Assistant can request a target temperature through display v0.12.1's Climate entity; the display uses its normal validated controller link. HA has no direct controller/MQTT mode or servo commands. Neither HA nor a broker is required for the thermostat engine. See [standalone MQTT monitoring](../docs/STANDALONE_MQTT_MONITORING.md).

Paired radio configuration from the display webpage and the 2 MiB app partition remain. Configuration changes restart in Hold with bench output released. Controller firmware updates still use USB. Bench output interlocks, simulation default and explicit arming/synchronization are retained.

The Super Mini v0.5.9 build advertises **CottageThermoController.local** using mDNS, and exposes live hostname/LAN/ESP-NOW diagnostics plus a browser-only TEST capture/export. See [mDNS auto-discovery and bench test](../docs/CONTROLLER_MDNS_AUTO_DISCOVERY.md).

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

## HW-747 Super Mini 4 MB bench firmware (v0.5.6)

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
existing controller configuration alone. It updates a previously saved Super Mini
servo pin from GPIO38 to GPIO5 while keeping other local settings. Future rebuilds must use this script
(or the same SDKCONFIG and build directory). Do not flash the ordinary
`controller/build` artifacts to this 4 MB board.

| Connection | Super Mini pad |
|---|---|
| AS5600 VCC | 3V3 |
| AS5600 GND | GND |
| AS5600 SDA | GPIO6 |
| AS5600 SCL | GPIO7 |
| Servo signal | Outside GPIO5 hole |
| Servo ground | GND (shared with its power supply) |
| Board power | USB-C, 5 V |

Use a suitable regulated 5 V supply for the servo. The board's 5V pad can only
be used for servo power if its USB power path and supply support the combined
load. B+/B- remain unused for these USB bench tests. The servo's supply is never
3V3. Solder all connections and inspect for bridges, around adjacent pads.

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

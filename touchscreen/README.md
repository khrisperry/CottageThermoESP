# Room thermostat client v0.2.0

A separately buildable ESP-IDF **6.0.1 / ESP32-S3** room client. Implemented now: direct authenticated controller communication, browser thermostat UI, setpoint changes, reconnect behavior, and external **SHT31 or SHT4x** probe reporting with CRC validation. No fabricated temperature is sent when the probe is missing or fails.

**The physical LCD/touch/LVGL board adapter is not implemented.** The project specification still names the Waveshare ESP32-S3-Touch-LCD-4B as a candidate; exact board revision, display/touch drivers and probe wiring must be confirmed before assigning GPIOs. This project can build now without a probe or LCD. Browser UI is served by the room ESP itself; use its IP printed in the serial monitor.

## Build/configure

From a VSCode ESP-IDF PowerShell terminal:

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP\touchscreen
# First build of this project only:
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
# Flash only after confirming the target board, flash size, USB console and its COM port:
# idf.py -p COMxx flash monitor
```

Under **Cottage room thermostat**, enter the controller pairing token from its USB monitor. Default network is the open `CottageThermoBench` AP and controller URL `http://192.168.4.1`. Both devices can alternatively use the same LAN (configure each SSID/password and controller address). Credentials stay in ignored `sdkconfig`; do not commit them. The default 8 MB flash/USB console profile must be checked against the actual room board before flashing.

Probe defaults to **disabled**, pins **-1**. When wired and verified, select `1` (SHT31, address 0x44) or `2` (SHT4x, address 0x44) and set the external probe's SDA/SCL. Use board-appropriate 3.3 V wiring/pullups and mount the probe away from screen/CPU heat. Readings are sent every 15 seconds. I2C failure/CRC mismatch/out-of-range reading sends nothing, allowing the controller's stale timeout to work. Failed network reports are discarded, not replayed after reconnect. SHT31 and SHT4x use separate measurement command/timing paths.

Open the room client's printed IP in a browser, enter the same pairing token and change the target. The client never stores the committed setpoint locally and cannot arm the servo or set the controller's operating mode. Controller restarts, unavailability and missing fresh readings appear explicitly; old room temperature is cleared from the UI on connection loss. Entering a target overrides a controller schedule until it is re-enabled there.

Transport is paired local HTTP for this attended development setup. Production encrypted pairing, MQTT, physical LCD integration and signed OTA remain separate work.

## Home Assistant panel (v0.3.0)

User confirmed Waveshare **ESP32-S3-Touch-LCD-4B** on 2026-10-20. The room client now embeds `/home` for Home/Laundry/Camera controls and forwards compact requests to the local HA bridge. See [../ha_bridge/README.md](../ha_bridge/README.md) for the exact mappings, secure local credential setup, Windows validation and optional Proxmox deployment. The physical 4B LCD/touch adapter is still pending. The furnace controller and its simulation/bench interlocks are unchanged.

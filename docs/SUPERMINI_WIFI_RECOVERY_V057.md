# Super Mini controller Wi-Fi recovery — v0.5.7 bench test

**Branch:** `fix/supermini-wifi-recovery-v0.5.7` (based on `feature/connectivity-climate-review`, not the older `main`).
**Hardware:** HW-747 ESP32-S3FH4R2 (4 MB flash), servo GPIO5; AS5600 SDA6/SCL7.
**Status:** Code staged for review and CI build; needs physical confirmation. Controller output remains SIMULATION by default; boot PWM is disabled.

## What changed

- A 15-second AP discovery window starts before the first saved-LAN connection attempt.
- While a phone is associated with `CottageThermoBench`, automatic station retries and ESP-NOW channel hopping pause. The AP is an open setup network; **captive DNS is not implemented**, so browse manually to `http://192.168.4.1`.
- After three unsuccessful LAN attempts with **no configured ESP-NOW peer**, the controller suspends station retries and channel search to maintain its setup AP until a profile save/restart.
- When encrypted ESP-NOW is configured, the controller **does not automatically suspend fallback discovery** following LAN failures. Instead, press and hold the HW-747 **BOOT button for three seconds after normal boot** to enter setup recovery. Do **not** hold BOOT during power-up: that selects the ROM downloader. The recovery session pauses ESP-NOW channel rendezvous, so only use it while attended and do not treat the resulting link as a verified furnace fallback.
- On the Super Mini only, the shared Wi-Fi/ESP-NOW transmitter is capped to 13 dBm (ESP-IDF quarter-dBm value 52), matching a previous ESP32-C3 bench stability setting. This is an **experiment**, not a confirmed root cause, and can reduce RF range.
- Serial diagnostics now log setup AP start, setup client joins/leaves, LAN disconnection reasons/failure count, and TX-power cap. `/api/status.network` includes `setup_ssid`, `setup_recovery`, `failed_connects`, `setup_clients`, and `wifi_max_tx_dbm`.
- Existing saved Wi-Fi profile, integrations/paired key, calibration, and servo safeguards are retained. Do **not** erase NVS.

## Build / flash from ESP-IDF 6.0.1 PowerShell on Windows

```powershell
cd C:\Users\kperry\Documents\GitHub\CottageThermoESP
git fetch origin
git switch fix/supermini-wifi-recovery-v0.5.7
git pull --ff-only
.\tools\build-controller-supermini.ps1
# Check the actual COM port in Windows Device Manager; COM13 is only an example:
.\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM13
```

The script maintains separate `controller/sdkconfig.supermini` and `controller/build-supermini` and does not reflash the room thermostat.

## On-device verification

1. Observe version **0.5.7**, `Setup access point started`, `Super Mini Wi-Fi max TX power: 13.00 dBm`, and `Setup AP discovery window 15 s`.
2. Within the 15-second discovery window scan for **CottageThermoBench** on a phone. Join it despite its no-Internet warning; open **http://192.168.4.1**. No automatic portal pop-up is expected.
3. While the phone is connected, observe `Setup client joined`; LAN reconnect attempts should pause. If the SSID never appears even during the discovery window, inspect supply, antenna/board integrity, and radio/AP event logs before altering the home Wi-Fi network.
4. If no peer is configured, allow three failed LAN attempts; confirm `setup_recovery: true`, channel remains stable, and provisioning works.
5. If a peer *is* configured and setup AP cannot be reached, hold **BOOT for three seconds after startup**. Confirm recovery log and join the AP. Saving credentials through the controller web UI restarts the controller.
6. Confirm encoder still reports a valid angle, servo does not move on boot/reconnect, and thermostat pairing settings persist.
7. If the AP works but LAN repeatedly fails `reason 2`, inspect SSID/password, AP band/security/PMF and AP client events **without changing or disabling your shared network**.
8. If 13 dBm works, separately compare a higher TX power in another controlled bench build before attributing failure to RF power.

## Important constraints

ESP-NOW and STA/AP Wi-Fi use the same 2.4 GHz radio. When the phone is attached to the setup AP or recovery mode is active, channel rendezvous is deliberately suspended; test the offline control path again after the phone disconnects and LAN service is restored. This is a bench recovery behavior, **not a production-validated fallback policy**.

Build artifact (if GitHub Actions succeeds): Actions → **Super Mini controller test firmware v0.5.7** → latest successful run → `supermini-v0.5.7-flash-images`. The zip contains the separate ESP-IDF flash files and arguments. Do not flash a combined image without checking offsets and the generated `flasher_args.json`.

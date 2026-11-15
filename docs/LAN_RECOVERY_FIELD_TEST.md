# 60-second LAN recovery — attended field test (V1.0.4)

1. On the thermostat's existing firmware OTA page, install only the V1.0.4 Waveshare 4B thermostat application image from a **successful** GitHub Actions build. Do not flash the controller, erase calibration/NVS, or alter ESP-NOW pairing.
2. Open Settings > Link diagnostics. Record starting `lan_grace_started`, `lan_grace_recovered`, `lan_grace_expired`, `switched_to_espnow`, and `link_http_fail` counters.
3. Operate the furnace normally for several cycles; make no deliberate controller network disruption without onsite supervision. Look for transient LAN HTTP failures that begin/recover within the 60-second window without switching to radio.
4. For an observed >60-second controller LAN interruption while the thermostat Wi-Fi remains connected, an ESP-NOW probe is allowed after the grace period; do not assume it succeeded without valid paired radio acknowledgement.
5. True panel Wi-Fi disconnection is exempt and can fall back immediately. Normal setpoint/Away writes are never silently queued or repeated. A failed UI command must be confirmed and deliberately retried.
6. Download `/api/controller/diagnostics` JSON and `/api/learning.csv` *before reboot*; both detailed traces are RAM-only.
7. Verify controllers remain aligned/Live after network recovery, and compare stages with the existing Home Assistant furnace electrical measurements. Maintain the mechanical override and an independent safe heating backup.

# Controller mDNS + touchscreen auto-discovery — bench test

## Waveshare 4B v0.13.9: Wi-Fi rejoin after temporary network isolation

The controller firmware **v0.6.5** correctly continues serving HTTP and
paired encrypted ESP-NOW during a temporary thermostat Wi-Fi block. Previous
touchscreen **v0.13.8** repeatedly logged
`mDNS returned 192.168.60.85 but status check failed: err=-1 HTTP=200`.
The controller is not refusing the connection: `room_controller.c` used an
**1800-byte stack buffer** for the controller's growing `/api/status` reply.
When truncated, `room_http()` rejects the incomplete response even with
HTTP 200. Failed discovery never promotes an otherwise healthy recovered Wi-Fi
link, so ESP-NOW remains in use. Multicast discovery can also temporarily
return `ESP_ERR_NOT_FOUND` during retries without indicating a broken Wi-Fi
association.

**Fix:** the thermostat now fetches complete controller diagnostics into a
bounded **8 KiB heap buffer** outside the controller task's stack, validates
the exact controller hostname and (when configured) paired station MAC, and
retries discovery after network recovery. The paired encrypted ESP-NOW
fallback is unaffected. If the reply remains incomplete, the transport logs
its received/body-capacity/content-length values to separate HTTP truncation
from connectivity problems. The new regression test compiles the actual
discovery probe and simulates HTTP responses larger than 1800 bytes,
temporary isolation, restoration without reboot, malicious MAC/name mismatch,
malformed and oversized replies.

**Flash only the 4B thermostat firmware v0.13.9**, keeping the local SHT45
sensor settings and secrets. Do not erase either device's NVS or reflash the
already working v0.6.5 controller.

After updating, use the touchscreen's own `/settings` Controller section in
`auto` mode and the read-only Controller Test. During a supervised bench test,
temporarily block **only the thermostat's Wi-Fi station**, verify ESP-NOW
continues acknowledging samples and setpoints, then unblock it. Expect the
touchscreen to regain `192.168.60.x`, log
`Verified CottageThermoController.local at ...` and
`Wi-Fi controller link restored`, without a restart. Confirm that both
read-only Wi-Fi and ESP-NOW tests succeed. Controller physical position
safety/Go mode remains exactly as before.

If mDNS multicast is unreliable even after the fix, the known LAN IP
`http://192.168.60.85` may be configured explicitly in the thermostat's
Controller settings for diagnostics, but that IP is DHCP dependent and
should not be the permanent production strategy. The control protocol
continues validating reply identity/leases and sends mutations through
exactly one transport; it does not retransmit uncertain setpoint writes.

Controller v0.5.9 (ESP32-S3 Super Mini 4 MB) advertises the mDNS host CottageThermoController.local, with _http._tcp and _cottagethermo._tcp on port 80. It retains the DHCP hostname CottageThermoController.

Controller v0.5.9 removes the experimental Super Mini 13 dBm TX cap and uses the normal driver/PHY/country-limited maximum. Startup logs the actual maximum; the controller website reports it in `network.wifi_max_tx_dbm`. No touchscreen update is needed for this power change.

Touchscreen Waveshare 4B v0.13.8 resolves this host using mDNS. Under /settings > Controller, save auto (recommended), CottageThermoController.local or an explicit IP/URL. The auto mode confirms the reported controller hostname and, when configured, the expected controller station MAC. Existing IP settings remain preferred until they fail, after which a verified mDNS resolution is attempted. Address changes never rewrite NVS automatically.

mDNS is multicast UDP: normally both devices must have multicast reachability within the same VLAN/subnet, or a configured Omada mDNS relay/reflector must bridge them. Do not disable shared Wi-Fi or disturb the household network to test. ESP-NOW is still separate and uses its existing paired station MAC/key/channel behavior.

Flashing from an ESP-IDF 6.0.1 PowerShell terminal, repo root:

1. git fetch origin
2. git switch feature/controller-mdns-discovery-web
3. git pull --ff-only
4. .\tools\build-controller-supermini.ps1 -Flash -Monitor -Port COM13
5. .\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14

Replace COM13/COM14 with your actual device ports. Keep -Bme280Test only for the current BME/BMP bench-sensor profile; omit if using the intended permanent probe. The build scripts preserve existing local SDKCONFIG and secrets. Never erase NVS or flash one board's image to the other board. CI-produced room-panel images are built without your real secrets; use the local build for the correct sensor and secrets.

Acceptance:
- After flashing the Super Mini, look for a log confirming mDNS advertised, and network.mdns_ready=true in its controller website diagnostics.
- On a device attached to the same multicast-capable LAN, open http://CottageThermoController.local/ and verify the controller website.
- Controller webpage shows hostname, LAN/mDNS/setup network status, encoder feedback, and a disposable test capture/export. The browser recorder samples every 3 seconds only while its page remains open; firmware history CSV still samples once a minute.
- After flashing the room panel, open its webpage /settings > Controller and save auto. Restart and check discovery hostname, resolved IP, active origin, and verified status. Test Wi-Fi and ESP-NOW individually.
- If saved manual IP is stale, the panel tries mDNS after repeated failures. No radio pairing or servo commands are changed by discovery.
- Servo must remain disabled on boot. Bench data is disposable; no adaptive furnace configuration is applied.

Operational caveat: a mDNS + /api/status identity match is an ordinary LAN check, not cryptographic server authentication. Paired ESP-NOW still uses its private key. Treat the controller LAN as a trusted network.

## v0.5.9 Wi-Fi power comparison and hostname checks

- Confirm controller version 0.5.9 and `Super Mini Wi-Fi max TX power (driver default)` in the serial log. The measured maximum should exceed the former 13 dBm cap, subject to the driver/PHY/country limits.
- After DHCP, the log prints both the current IP URL and `http://CottageThermoController.local/`. The printed `mDNS registered=yes` means local initialization succeeded; it does not confirm that another device can resolve the name.
- Test the printed IP URL first, then the full `.local` URL from the same computer. If IP works but the name fails, investigate multicast/name resolution separately from association. Bare DHCP hostname resolution depends on the router/DNS setup.
- From Windows PowerShell, `ping CottageThermoController.local` shows whether the name resolves (ICMP replies are a separate check). `curl.exe --max-time 10 http://CottageThermoController.local/api/status` tests name resolution plus HTTP. Compare with the current IP URL; do not assume the previous DHCP address is permanent.
- Preserve Wi-Fi credentials, calibration and ESP-NOW pairing; do not erase flash/NVS. Compare reconnect behavior and actual HTTP access after a restart. Hardware RF behavior and hostname reachability require this local check.

# Controller links — controller 0.5.1 / room panel 0.13.1

The AtomS3 Lite uses GPIO38 for the existing servo. Magnetic position feedback is not installed or required to test communications. Boot keeps PWM disabled and position unknown. The existing simulation/live-bench calibration, arming and synchronization gates remain in force. Communication recovery cannot arm the servo. HA requests a target through the wall thermostat; it has no direct servo access.

## Build and flash

Use the existing ESP-IDF 6.0.1 PowerShell terminal, at the repository root:

```powershell
git switch feature/connectivity-climate-review
git pull --ff-only
.\tools\build-local.ps1 -Project controller
cd controller
idf.py -p COM13 flash monitor
```

COM13 is the previously used controller port. Use the AtomS3 Lite's current port if Windows changed it. No erase-flash is needed. After leaving the controller monitor, return to the repository root and update the panel:

```powershell
cd ..
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

Retain `-Bme280Test` only for the existing test sensor configuration. It still supplies local display readings only, not furnace control samples. Existing SHT31/SHT4x control sensing is unchanged.

## Join the LAN and pair

1. Join `CottageThermoBench` and open `http://192.168.4.1`. The page opens directly; no local access token is required.
2. In **Wi-Fi & thermostat connection**, enter your LAN SSID/password and save. The controller restarts. Its DHCP hostname is `CottageThermoController`. The bench AP remains available if LAN connection fails. A blank password retains the existing password only for the same secured SSID; check Open network for an open LAN. Blank SSID explicitly selects AP-only operation.
3. Reconnect to the controller page. Record its LAN IP and **station MAC**. The page shows the channel, radio readiness, recent paired contact and HTTP link request count. Position feedback explicitly says Not installed.
4. Open the thermostat webpage `/settings` → **Controller**. Save the controller LAN IP/URL. After the display restarts, verify the controller is reachable over Wi-Fi.
5. On the thermostat webpage → **ESP-NOW**, enable paired fallback, enter the controller **station MAC**, and enter a private 32-hex-digit key. Generate one locally with `python -c "import secrets; print(secrets.token_hex(16))"`. Save. The display sends its station MAC and the shared key to the controller over the local HTTP link; the controller restarts to apply them. Wait for **Radio pairing sync: Applied** on Overview.
6. Click **Test Wi-Fi and ESP-NOW**, available under Controller and ESP-NOW. Expect an independent controller acknowledgement for each. This only sends status requests; it neither moves the servo nor changes the selected transport or its cached lease. Settings must already be saved; the button tests the active configuration.

Encrypted ESP-NOW requires both radios on the same channel. With both connected, their APs must use the same 2.4 GHz channel to pass the radio test. Disconnected devices search channels and pause search when a bench/setup AP client is attached. Disconnect phones/laptops from the devices' setup APs before outage tests. A one-time radio probe proves the exchange at that moment; radio status can return to standby while normal control traffic uses Wi-Fi.

## Omada outage test

Leave USB monitors attached so the test does not depend on access to the blocked device's webpage. Record times; physical handover timing has not been established by host tests.

| Test | Action in Omada | Expected result to verify on hardware |
| --- | --- | --- |
| Baseline | Both allowed | Wi-Fi controller status succeeds; independent radio test acknowledges |
| Controller loss | Block controller client | Thermostat changes to ESPNOW after a valid paired reply; controller remains responsive through radio |
| Recovery | Unblock controller | Automatic Wi-Fi reconnection; thermostat returns to Wi-Fi and refreshes HA state |
| Thermostat loss | Block thermostat client only | Local thermostat UI uses ESPNOW; HA features become unavailable during fallback |
| Both lose LAN | Block both clients | Direct channel rendezvous and paired replies continue without router/broker/HA |
| Sustained outage | Keep both blocked for one hour | Watch for restart loops, stale status and loss of paired replies |
| Final recovery | Unblock both | Automatic return to Wi-Fi, no re-pairing, no servo arming or homing |

Observe actual association/IP state in the USB logs: a block that only prevents Internet access while local device-to-device HTTP remains available will not exercise fallback. Keep the devices powered. The panel logs `Controller link switched to ESP-NOW` and `Wi-Fi controller link restored`; controller logs include disconnect reasons and LAN IP acquisition. First verify communication/status and target round trips without arming output. A target accepted with missing calibration/position can be stored while physical motion correctly remains blocked.

Software validation evidence is recorded in the change description. Mock transport tests do not establish actual radio range, AP behavior, channel rendezvous time or servo position.

## Host validation for this change

Three engine/protocol/network-profile tests and seventeen native service tests pass. The focused tests execute the real room transport and settings handlers with mocked HTTP/radio/NVS, including independent path acknowledgements, rejected status replies, no control mutations from diagnostics, no transport/lease changes, ambiguous mutation no-retry, controller reboot, an hour outage and Wi-Fi recovery. Network tests cover field bounds, retained passwords, new-network password requirements, open networks and AP-only configuration. Both embedded webpage scripts pass JavaScript syntax checks.

The native LVGL page-rendering fixture was not run in this checkout because the JPEG managed dependency was unavailable; the other seventeen native service tests passed. These are host checks, not an ESP-IDF firmware build or physical radio verification. Build/flash both projects locally with the commands above.

Controller 0.5.1 / room panel 0.13.1 removes local access-token requirements and controller-token configuration. Focused transport, settings and keepalive tests pass without local credentials, and the embedded page scripts pass syntax checks. Existing NVS profiles retain their layout; no erase-flash is needed.

## Panel 0.13.2 — intermittent controller status

Controller HTTP polling now reuses a serialized persistent connection, with controller URL/error/HTTP status in failure logs. A failed HTTP connection is discarded for the next request; mutations are never automatically replayed. The panel retains only its last acknowledged climate snapshot until the existing ten-second display expiry. Missed polls do not update that timestamp or extend the separate controller command lease. ESP-NOW reply age is measured from the start of its own exchange, excluding time spent on a failed HTTP probe.

Host checks pass against the actual LVGL display source, transport implementation and room controller code: a missed poll retains status briefly, continued failure expires it, recovery restores it, ten local POSTs reuse one connection, a failed POST is not replayed, a fresh radio reply after a slow HTTP failure is accepted, and an excessively delayed radio reply is rejected. This does not establish radio range or hardware handover timing. Update only the panel to 0.13.2 using the build/flash command above.

If the boards connect to different AP channels, ESP-NOW cannot exchange while both remain associated on those channels. For the first bench test connect both through the same 2.4 GHz AP. `Encrypted paired ESP-NOW ready` confirms initialization, not a successful peer acknowledgement.

## Panel 0.13.3 — quiet fallback and automatic Wi-Fi recovery

The MQTT worker now stops the broker client during Wi-Fi loss or direct fallback, which stops its internal reconnect attempts. It restarts the existing client when Wi-Fi returns and the normal controller link recovers, then refreshes subscriptions/discovery/state. Incoming MQTT target requests and publishing are gated while paused. When the LAN remains reachable at the transition, the panel makes one final retained offline availability update; an actual Wi-Fi outage uses the broker Last Will. HA polling exits before allocating a response or making requests while offline/fallback, and invalidates old entity availability. Ships and camera already gate their requests on that state. In-flight operations may finish/fail once at a transition.

Wi-Fi recovery continues separately from those services. The timeout path previously depended on receiving a new disconnect event: when the driver was already disconnected it could leave the application stuck in connecting and repeat disconnect every 250 ms. The timeout now clears application connecting state and schedules a new attempt regardless of whether an event follows. Authentication/IP attempt timeouts are checked independently of the next retry deadline. Logs now identify Wi-Fi reconnect attempts and timeout recovery. With paired fallback enabled, retries leave roughly twenty seconds between attempts to preserve direct-radio time.

Validation: host MQTT lifecycle tests cover offline and fallback stop/no-retry, publication/command rejection, resume and rediscovery. Direct HA regression tests pass. The Wi-Fi retry policy test covers timeout with no disconnect callback, repeated idle ticks, fresh retry and subsequent failure/retry. The three changed implementation files (Wi-Fi setup, MQTT, HA) also compile successfully with the ESP-IDF 6.0.1 ESP32-S3 compiler and existing Waveshare 4B build headers/configuration. This is compile-only validation of those objects, not a full firmware link or a physical block/unblock test.

After updating the panel, block Wi-Fi again: confirm the ESP-NOW status and quiet external services. Unblock without restarting. Expected logs include `Wi-Fi reconnect attempt`, `Room UI: http://...`, `Wi-Fi controller link restored` and `Broker connected`. Then verify fresh HA/Ships status returns. ESP-NOW carrying status/target messages does not establish magnetic feedback or physical furnace readiness.

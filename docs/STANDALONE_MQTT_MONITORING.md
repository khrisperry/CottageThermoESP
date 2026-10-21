# Standalone thermostat and Home Assistant Climate

Display **0.12.1**, controller **0.4.1**. Home Assistant can adjust the thermostat target; it has no direct servo control. This supersedes the display 0.12.0 monitoring-only decision.

## Ownership and standalone operation

The wall thermostat supplies local probe reports and target requests to its paired servo controller. The controller runs the thermostat engine and movement interlocks. HA and MQTT are optional: losing either does not stop local sampling, touchscreen temperature changes or the direct controller link. Lights, laundry, blinds, vacuum and camera remain optional direct HA features.

The display owns the MQTT connection and reuses the broker profile saved on its webpage. The controller has no MQTT client or subscriptions. HA publishes a requested temperature to the **wall thermostat**; the display validates it and queues exactly the same target-only command used by the touchscreen. The existing paired HTTP/ESP-NOW path and controller validation apply. Requests cannot arm or synchronize output, alter mode, run calibration or command PWM/servo position.

The controller remains authoritative for the accepted target. MQTT state reports the confirmed target, rather than optimistically claiming success. Ambiguous/rejected commands are not replayed automatically. Controller link loss prevents new targets and probe reports; existing freshness rules and servo interlocks still apply. ESP-NOW needs both devices paired/configured and still requires physical outage testing.

The BME280 remains a **local test sensor**, visible locally and through HA telemetry. It is not sent to the controller as the production control probe. Production SHT31/SHT4x configuration and physical calibration/mechanical gates remain unchanged. Automatic servo output defaults to simulation.

## Discovered entities

All entities share the wall display's stable MAC-derived device identity (`cottage_room_<station MAC>`).

| Entity | Behavior | Availability |
| --- | --- | --- |
| Thermostat (Climate) | Adjust target 45–80°F, UI step 0.5°F; confirmed target and local temperature | MQTT connected and fresh controller target |
| Room temperature | Read-only local reading | MQTT connected and local probe fresh |
| Target temperature | Read-only reported target | MQTT connected and fresh controller target |
| Controller connection | Read-only direct-link status | MQTT connected; reports connected/disconnected |

The Climate discovery exists even with the controller offline, but the Climate entity is unavailable until a fresh controller target arrives. Room temperature remains independently available when its probe is fresh.

The Climate entity exposes **temperature adjustment only**. Mode, power, preset and servo command topics are absent. In controller Thermostat mode HA reports `heat`; Hold/Manual/Direct modes report unknown rather than falsely claiming that Hold switches off the furnace. The actual controller mode and simulation flag remain visible as attributes. Changing the target does not switch modes, enable physical output or assert that the furnace is currently heating.

Discovery uses the configured prefix (default `homeassistant`). State lives under `cottage/thermostat/cottage_room_<MAC>/state`, refreshed every two seconds with retained availability and Last Will. The three monitoring sensors expire after 45 seconds without updates. Climate availability also follows the controller freshness checks and MQTT Last Will.

Only two topics are subscribed: HA birth (`homeassistant/status`, for rediscovery) and the display's own `.../target/set`. Target payloads must be complete finite decimal temperatures in range. Retained, duplicate, fragmented, malformed and out-of-range commands are ignored. Unavailable, stale, busy or full-queue requests are rejected; accepted requests expire under the same ten-second queue limit as local changes. No command is sent from the MQTT callback itself.

## Configure and update

1. Pull `feature/connectivity-climate-review` and build display 0.12.1. Flash over USB or upload the application bin through the existing device webpage OTA process.
2. Open `http://DEVICE_IP/settings` → **MQTT thermostat**. Existing broker settings are reused. Enable MQTT, save a broker host/port and credentials, and let the display restart. A plain LAN broker uses `mqtt://HOST:1883`; verified TLS uses `mqtts://HOST:8883` and requires valid system time.
3. The device Connections page and webpage show the display's broker connection independently of controller availability. Logs use `room_mqtt`, including broker connection/rejection and accepted/rejected thermostat target requests. Saved passwords are never returned by the webpage.
4. **Republish HA discovery** queues all four entities on an established broker connection without needing the controller. HA must use the same broker. The existing monitoring sensors retain their identities; the Climate entity is added to the same device.
5. Controller 0.4.1 stays unchanged. Update any older controller that previously ran MQTT to remove its legacy direct command path, and remove obsolete controller-owned retained climate discovery/HA entities if present.

Confirmed display port and BME280 test build:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

For webpage OTA, omit `-Flash -Monitor -Port COM14`, then upload `touchscreen/build-4b/cottage_thermo_room_4b.bin`. Do not upload a bootloader or merged image. The initial USB partition migration must already be installed.

## Verification

Host tests exercise production MQTT parsing, Climate discovery, the real native target queue, confirmed-state reporting, invalid/retained/duplicate/fragment guards, offline/busy/expired requests, reconnect/authentication diagnostics, and standalone local commands without HA. Browser checks cover configuration. The 4B firmware targets ESP-IDF 6.0.1. Live HA temperature changes, broker connectivity, radio handover and physical servo behavior remain device acceptance checks.

# Standalone thermostat and MQTT monitoring

Display **0.12.0**, controller **0.4.1**. This is the current architecture, superseding earlier proposals for an HA-controlled climate entity.

## Ownership

The wall thermostat sends local probe reports and local setpoint requests directly to the paired servo controller. The controller runs the thermostat engine and servo interlocks. Home Assistant and MQTT are optional monitoring services; neither participates in deciding or commanding servo movement. HA lighting, laundry, blinds, vacuum and camera continue through the display's optional direct HA integration.

The display now connects to the broker itself using the MQTT profile already saved on its webpage. It no longer forwards broker credentials to the controller. It can connect even when the controller is offline. The controller has no MQTT client, subscriptions or MQTT command handler. Paired HTTP/ESP-NOW communication is independent of HA and the broker.

Losing HA or MQTT does not block local temperature sampling or direct thermostat commands. Losing the controller link still prevents delivery of new targets and fresh probe reports; existing freshness rules and movement interlocks apply. ESP-NOW fallback requires both devices to be paired and configured and still needs physical outage testing.

The BME280 remains a **local test sensor**, visible locally and through read-only telemetry. It is not sent to the controller as the production control probe. Production SHT31/SHT4x configuration and the physical servo/calibration gates remain unchanged. Automatic servo output still defaults to simulation.

## Home Assistant entities

All entities belong to the wall display's stable MAC-derived device identity (`cottage_room_<station MAC>`).

| Entity | Source | Availability |
| --- | --- | --- |
| Room temperature | Fresh local probe reading | Display MQTT connected and probe fresh |
| Target temperature | Fresh controller-reported target | Display MQTT connected and controller fresh |
| Controller connection | Direct thermostat/controller link | Display MQTT connected; reports connected/disconnected |

There is **no controllable climate entity, setpoint command topic, mode command topic or servo control**. The only MQTT subscription is `homeassistant/status`, used to republish discovery when HA starts. Other inbound messages are ignored. State includes source/freshness and simulation diagnostics so test readings remain identifiable.

Discovery uses the configured prefix (default `homeassistant`). Telemetry lives under `cottage/thermostat/cottage_room_<MAC>/`. State is retained and refreshed every two seconds. A retained Last Will marks the display offline; entity state expires after 45 seconds without updates. Controller and probe freshness are evaluated separately, so a working room probe remains visible with the controller offline.

## Configure and update

1. Pull `feature/connectivity-climate-review` and build/flash display 0.12.0, or build it and upload the application bin through the existing local OTA webpage.
2. Open `http://DEVICE_IP/settings`, choose **MQTT monitoring**, enable monitoring and save the broker URI/credentials. Existing saved settings are reused automatically. A plain LAN broker uses `mqtt://HOST:1883`; verified TLS uses `mqtts://HOST:8883` and waits for valid system time.
3. Saving restarts the display. Its Connections screen and webpage now show the display's actual broker connection, independent of controller availability. Logs start with `room_mqtt: Connecting to broker ...`, then connection success or broker/socket error details. Passwords are not logged or returned by the webpage.
4. **Republish HA discovery** queues discovery on an established broker connection. It does not require the furnace controller. HA needs MQTT configured against the same broker.
5. Update the controller to 0.4.1 over USB when ready. If an older controller previously had MQTT enabled, its old command path remains until that controller firmware is updated. Remove any obsolete retained climate discovery configuration/old HA climate entity after migration; the new display creates only read-only sensors.

For the confirmed display port and current BME280 test build:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Bme280Test -Flash -Monitor -Port COM14
```

For webpage OTA, omit `-Flash -Monitor -Port COM14`, then upload `touchscreen/build-4b/cottage_thermo_room_4b.bin`. Do not upload a bootloader or merged flash image. The initial USB partition migration must already be installed (the user's 0.11.2 boot log confirms OTA slots).

## Verification

Native tests exercise direct thermostat setpoint changes with HA offline, fresh local temperature with the controller offline, broker configuration/Last Will, discovery and reconnect, stable entity identifiers, stale sensor handling, authentication failure diagnostics, and ignored command messages. Browser tests cover the configuration page. Firmware builds target ESP-IDF 6.0.1 for both devices. Real broker connectivity, cross-device fallback and servo operation remain physical acceptance checks.

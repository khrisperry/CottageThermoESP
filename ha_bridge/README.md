# Optional legacy PC Home Assistant test helper

**ESP v0.5.0 connects directly to HA using `touchscreen/secrets.h`. It does not use this bridge or its client token. Follow [the native setup](../touchscreen/README.md) instead.** This helper is retained for the already-tested standalone PC webpage. The entity JSON and HTML here are also build inputs shared by the ESP; that does not require running the bridge service.

This host service runs on a Windows PC for initial validation, then can run beside the ship tracker in the Proxmox LXC. Home Assistant authentication, TLS and JPEG scaling stay on the host. The ESP receives a small state document and sends allowlisted requests. **This module has no furnace servo or main thermostat control endpoint.**

Target display confirmed by Khris: **Waveshare ESP32-S3-Touch-LCD-4B**. The current implementation provides a browser Home/Laundry/Camera panel and an ESP-IDF room-client proxy. Version 0.4.0 now also includes a native 4B LCD/touch/LVGL panel; see `../touchscreen/README.md` for its dedicated build/configuration instructions. The native display has compiled and passed host UI checks; physical board validation remains pending.

## Supplied configuration

Home Assistant: `https://cottageha.greenledges.net:8123`.

| Key | Home Assistant entity | Available requests |
|---|---|---|
| deck | `light.cottage_porch` | On / Off |
| breezeway | `light.breezeway_string_light` | On / Off |
| blinds | `cover.roller_shade_f1` | Open / Stop / Close, when supported |
| washer1 | `sensor.washer_1_status` | Read only |
| washer2 | `sensor.washer_2_status` | Read only |
| dryer1 | `sensor.dryer_1_status` | Read only |
| dryer2 | `sensor.dryer_2_status` | Read only |
| kids_heat | `input_boolean.kids_heat` | On / Off |
| kids_temp | `sensor.cottage_kids_temperature_and_humidity_temperature` | Read only, retains reported °C/°F unit |
| vacuum | `vacuum.k11_vacuum_9d68` | Start / Stop, when supported |
| camera | `camera.cottagecamptz_fluent` | On-demand live image view |

Kids heat controls the **input_boolean** supplied by the user, not a climate entity. Its associated HA automation remains responsible for the radiator. Kids temperature is never forwarded as an input to the Williams furnace controller. The bridge has no SwitchBot mapping yet, no primary thermostat sensor substitution and no automatic furnace actuation.

## Test on Windows now

1. In Home Assistant, open your user profile → **Security** → **Long-lived access tokens**. Create a token for this bridge, using a dedicated account where practical. Do not paste the token into chat, a command-line argument, source code or a committed file.
2. From repository root in PowerShell:

   ```powershell
   powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\ha_bridge\start-local.ps1
   ```

   The helper makes a local Python virtual environment, installs requirements, then asks for the HA token with hidden input. The token lives in the child process environment and is removed by the helper on exit.
3. Open **http://127.0.0.1:8087**. Enter the temporary **bridge token** printed in that terminal. This is a separate client credential, not the Home Assistant token. No actuator request is sent until a user presses a control.
4. Confirm all eleven entities resolve, their actual states and temperature unit are correct, and controls are enabled only when HA reports the required services/features. Test lights first. Commands affect your real devices. A successful service response is followed by a state read; the UI never assumes physical completion.
5. Keep the terminal open during testing. Ctrl+C stops the bridge. The default bind is loopback only; this initial test needs no inbound firewall rule or ESP board.

For persistent credentials, set `HA_TOKEN` and a stable `COTTAGE_BRIDGE_TOKEN` in a private service environment. `.env.example` lists supported settings but **the bridge does not automatically load .env files**. Use your process manager/PowerShell environment or systemd `EnvironmentFile`. The Windows helper prompts only when HA_TOKEN is absent, and removes it from its own process environment on exit.

## Run in Proxmox later

Install this checkout to `/opt/CottageThermoESP`, create `ha_bridge/.venv`, and install `ha_bridge/requirements.txt`. Use the included `deploy/cottage-home.service` as the service definition. Put credentials in `/etc/cottage-thermo/ha.env`, owned by root with mode 600:

```text
HA_TOKEN=<your HA long-lived access token>
COTTAGE_BRIDGE_TOKEN=<separate random 32-character-or-longer client token>
COTTAGE_BIND=0.0.0.0
COTTAGE_PORT=8087
```

Generate a client token with `python -c "import secrets; print(secrets.token_hex(16))"`. Configure local firewall/VLAN access for the display and test clients. This development bridge is a local HTTP endpoint by default; keep it on the trusted LAN or terminate HTTPS at the existing reverse proxy. Do not port-forward it publicly. Upstream HA always uses verified HTTPS; a private root CA can be supplied with `COTTAGE_CA_FILE`. Redirects are rejected instead of forwarding credentials to another host.

No deployment or service installation is performed automatically. Actual LXC address and HA credentials have not been supplied to the coding environment.

## ESP room-client setup

The v0.5.0 room client uses direct authenticated HA REST requests. Old `ROOM_HA_BRIDGE_URL` and `ROOM_HA_BRIDGE_TOKEN` settings have been removed. Follow `../touchscreen/README.md`; the descriptions below apply only to this optional PC helper.

## Behavior, limits and endpoints

- Every 5 seconds, read the eleven configured entities through HA's state API. Refresh service availability every minute. Data older than 30 seconds, unknown/unavailable entities and detected connection failures disable controls. A global HA connection error invalidates cached availability immediately. One missing entity leaves the others usable.
- Fixed explicit requests only: lights/input_boolean turn_on/turn_off; cover open_cover/stop_cover/close_cover; vacuum start/stop. No arbitrary service/entity calls, toggle, shell commands, writes to HA's state API or startup mutations.
- Commands carry a request ID. The last 128 IDs are held for this bridge session so a duplicate does not repeat a possibly executed request. No automatic command retry or reconnect replay. A timeout can leave an **unknown outcome**; inspect current device state before intentionally sending a new request. Deduplication clears on bridge restart and is bounded to the last 128 requests.
- Laundry shows the four provided state strings. It does not infer completion, percentage or time remaining from power or elapsed time.
- Camera is an **on-demand low-rate live view**, up to one JPEG frame/second, resized to at most 480×320 on the host, maximum 96 KB per frame. The UI polls images only while Camera is visible and stops on Close, page change or browser backgrounding. It clears the old picture on connection failure. **Full-motion HLS/WebRTC video is not implemented.** Camera attributes/tokens are never returned to the panel.
- Restart begins with unknown states, then reads HA afresh. It never replays a control request. The independent furnace controller retains its own saved thermostat settings.

All API endpoints require `X-Cottage-Token`. Query-string tokens are rejected. No token is included in the HTML.

| Method | Endpoint | Purpose |
|---|---|---|
| GET | `/` or `/home` | Browser panel (no credentials embedded) |
| GET | `/api/ha/status` | Compact connection/entity state document |
| POST | `/api/ha/command` | `{"key":"deck","action":"on","request_id":"<unique ID>"}` |
| GET | `/api/ha/camera` | One scaled JPEG; polled only for visible live view |

On the room ESP, `/home` and these three API paths are also available, backed directly by the ESP HA client. Its `X-Cottage-Token` is the existing **room pairing token**. The HA bearer is read from local `touchscreen/secrets.h` and never returned to the browser. Thermostat routes `/api/status` and `/api/target` continue to use `X-Thermo-Token`.

## Validation

```powershell
python -m pip install -r .\ha_bridge\requirements.txt
python -m unittest discover -s .\tests\ha -v
```

Automated checks use simulated HA responses and a real local HTTP server, not the cottage HA instance. They cover exact entity/service routing, input_boolean versus climate handling, missing/unavailable/stale data, unsupported device features, duplicate and ambiguous commands, authentication, TLS verification/redirect handling, and on-demand resized JPEGs. Actual cottage access requires the locally configured token; no claim of live HA validation is made.

Browser smoke checks: run `npm install` in `tests`, then `npm run test:ha` (with a Playwright Chromium installation). They exercise mobile layout, control selection, ambiguous command results, laundry status, camera lifecycle and offline behavior. The room-client firmware was also compiled with ESP-IDF 6.0.1.

Primary API references: [HA REST API](https://developers.home-assistant.io/docs/api/rest/), [cover actions](https://www.home-assistant.io/integrations/cover/), [vacuum actions](https://www.home-assistant.io/integrations/vacuum/), [ESP HTTP client](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/api-reference/protocols/esp_http_client.html).

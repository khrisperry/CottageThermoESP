# Direct controller API v1 (firmware 0.2.0)

All `/api/*` requests require `X-Thermo-Token: <32-character pairing token>` from the controller's serial monitor. GET `/` serves the web UI without exposing the token. Tokens are persistent per controller; no default secret is shipped. Local HTTP is for attended development. The AP is open, not an encrypted production transport.

POST accepts one JSON object, maximum 4096 bytes. Incomplete, nonfinite, out-of-range values fail. Writes acknowledge only after settings commit, or after the requested session operation. Settings updates are idempotent and skip unchanged NVS writes. Invalid settings leave current runtime configuration intact. Room reports must be live acquisitions, never retained/replayed HA values. A successful command acknowledges acceptance, **not measured motion**.

| Method | Path | Content / behavior |
|---|---|---|
| GET | `/api/status` | Version, simulation, mode, effective target, selected source, fresh readings, unknown/measured position distinction, movement reason, arm time remaining |
| GET | `/api/config` | Complete committed configuration, map and schedule |
| POST | `/api/config` | Partial update: `mode`, `target_f`, `direct_level`, `secondary_enabled`, `dwell_ms`, `level_deadband`, `max_level_step`, `map`, `schedule`, `schedule_enabled` |
| POST | `/api/temperature` | `{"source":"room","temp_f":62.5,"age_ms":0}`; source only `room` or `switchbot`; no source timestamp clock synchronization needed |
| POST | `/api/clock` | `{"unix_s":<current UTC epoch seconds>}` for AP-only operation; local zone is Detroit |
| POST | `/api/sync` | `{"level":3,"confirm":"simulation"}`; live bench uses `observed_bench_position` and requires arming |
| POST | `/api/arm` | `{"confirm":"unloaded_bench"}`; explicit 15-minute physical bench session, no motion by itself |
| POST | `/api/pulse` | `{"us":1500}`; physical raw PWM, Hold only, armed session required, 1100–1900 inclusive; invalidates thermostat synchronization |
| POST | `/api/stop` | `{}`; disable PWM, disarm, clear synchronization, enter session Hold; never rotate to zero |
| GET | `/api/events` | Last 64 uptime-stamped event messages (RAM) |
| GET | `/api/history` | Up to six hours of minute samples as CSV (RAM) |

`mode`: `hold`, `manual`, `direct`, `thermostat`. Hold does not re-home or imply actual position. Manual disables PWM and clears synchronization. Stop sets Hold for this session; the stored operating mode remains available on reboot, while boot still blocks actuation through missing synchronization/arming.

Calibration `map`: array of 2–8 objects with `temp_f`, `level`, `pulse_us`; temperatures strictly increasing, levels strictly increasing within 1–6, pulse direction consistently increasing or decreasing within 1100–1900. Empty array clears calibration. Map changes invalidate synchronization. Defaults contain no example measurements.

Weekly `schedule`: 0–14 objects `{ "days":127,"minute":360,"target_f":68 }`. Days bit 0=Monday through bit 6=Sunday, so 127=every day. `minute` 0–1439. Entries sharing any day cannot overlap at the same minute. `schedule_enabled=true` requires at least one entry. Weekly predecessor selection carries forward, including before the first entry of a day. Manual `target_f` disables scheduling unless the same explicit update sets `schedule_enabled=true`.

Freshness: both sources expire after 90 seconds. `age_ms` includes known age since acquisition, must be an integer 0–90000. Negative, future-to-boot, duplicate/out-of-order and unsupported source samples fail. Room wins over secondary; enabled fresh sources differing by >5°F pause thermostat motion. Control is not permitted to replace these sources with Kids room, keg sensor, outdoor temperature or a cached HA value whose freshness is unknown.

`position_synchronized=true` means an explicit virtual/manual observation established this session's starting command reference. `position_measured` is always **false** until an actual encoder adapter is implemented. Never label commanded pulse as actual shaft angle.

Room client serves its own paired GET `/api/status` proxy and POST `/api/target` accepting **only** `target_f` 45–80. It cannot arm/synchronize/change mode. No automatic mutation retry queue exists. Repeating a setpoint update is idempotent.

## Room transport endpoint (controller v0.3.0)

`POST /api/link` requires the existing `X-Thermo-Token` and a JSON `packet` field containing the exact hex-encoded versioned `tl_packet_t` envelope from `shared/thermo_link/link_protocol.h`. Replies use the same field/envelope. This endpoint serves status, bounded target writes and fresh primary-probe readings only. HTTP 200 carries the application result in the reply packet; transport success is not command acceptance. HTTP and paired ESP-NOW use the same handler, boot nonce, short monotonic lease and sequence guard. Existing browser/bench endpoints are retained. See CONNECTIVITY_REVIEW.md for transport handover and setup; this endpoint adds no raw actuation or arming operations.

## Controller network setup (v0.5.0)

`GET /api/status` now includes `position_feedback: "Not installed"` and a `network` object with `hostname`, `station_mac`, `ssid`, `connected`, `ip`, `channel`, `password_configured`, `radio_ready`, `radio_online`, `radio_peer`, `http_link_requests` and optional `last_http_link_age_ms`. Radio online means recent authenticated peer contact, not a permanently selected transport. Passwords and radio keys are omitted.

`POST /api/network`, with the controller's `X-Thermo-Token`, accepts `{"ssid":"LAN name","password":"passphrase","open":false}`. An empty password retains the prior secret only for the same secured SSID. Open networks require `open:true`; blank SSID selects AP-only mode. Valid profiles persist independently of calibration. Changed settings release output, clear session synchronization, enter Hold and restart. The setup AP remains available. The response includes `ok` and `restarting`.

On the room panel, same-origin `POST /api/settings` with `{"section":"controller_test"}` performs separate read-only status exchanges over HTTP and ESP-NOW. It returns `wifi_ok`, `radio_ready`, `radio_ok` and a human-readable `message`. It does not save settings, restart, set targets, arm output or change the currently selected transport/lease. A busy link returns 409. Tests use saved configuration.

# Cottage thermostat V1.0.3 — controller link diagnostics

## Goal
Separate independent read-only learning HTTP GET failures from actual furnace control link switches to ESP-NOW. A missed learning GET is not proof of a Wi-Fi outage or fallback.

## Deploy
Install only the thermostat application V1.0.3 image from the successful diagnostic GitHub Actions build using the normal thermostat OTA webpage. Keep the existing controller firmware, calibration, ESP-NOW pairing, safety fallback and mechanical/manual override. This build does not change any PWM or furnace movement decisions.

## Download before a restart
- Thermostat Settings > Link diagnostics, or GET /api/controller/diagnostics — read-only JSON.
- GET /api/learning.csv — now includes learning_lan_reason and selected_control_transport.
- Event log and detailed learning CSV are RAM-only, so save both before rebooting.

## Diagnostic fields
- link_http_ok, link_http_fail: successes/failures of normal thermostat /api/link control path over LAN.
- switched_to_espnow, returned_to_lan: actual transport changes in the existing control status probe.
- learning_lan_ok, learning_lan_fail: independent read-only /api/status GET results.
- discovery_fail: unsuccessful mDNS lookup or discovered controller identity probe.
- Event entries record reason, ESP error, HTTP status, elapsed time, Wi-Fi state, STA/AP association, RSSI, channel and transport selected at the event.

## Common reasons
- controller_io_busy: read-only learning collector could not acquire the origin-copy mutex within 150 ms. No HTTP GET happened. Not a transport switch.
- controller_origin_missing: no resolved/configured LAN origin available yet.
- room_wifi_offline: room panel reports Wi-Fi unavailable.
- http_transport_error: read-only GET failed at the HTTP transport layer.
- http_non_200, empty_response, invalid_json, controller_identity_mismatch: read-only status response could not be used.
- link_http_fail: actual control LAN HTTP request failed. It might or might not result in an ESP-NOW transition.

## Onsite validation
1. Leave furnace control working normally. Confirm thermostat and controller are associated with their Wi-Fi APs; do not disable ESP-NOW fallback.
2. Save initial diagnostics JSON, run through normal heating cycles, then save JSON and learning CSV after a missed controller_lan reading.
3. Match CSV learning_lan_reason with events by device uptime. Count actual switched_to_espnow events before concluding the control link moved.
4. If controller_io_busy dominates, investigate diagnostic collector scheduling instead of changing controller Wi-Fi. If link_http_fail occurs, compare ESP error, HTTP status, RSSI, channel and Omada association records.
5. Do not enable thermal trim or modify safe furnace movement ranges based on diagnostic data alone.

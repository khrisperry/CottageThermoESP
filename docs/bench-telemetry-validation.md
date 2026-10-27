# Bench telemetry and UI validation — draft staging specification

Status: STAGED SPECIFICATION ONLY. Firmware changes, compilation, flashing and hardware validation are NOT complete.

## Scope
Thermostat owns control decisions and logs. ESP32-S3 Super Mini controller executes bounded servo positioning and reports AS5600 encoder feedback. ESP-NOW is the control path; MQTT/Home Assistant is supplemental and never directly drives the servo. No adaptive furnace control in this phase.

## Telemetry schema v1
Every record: schema_version=1, boot_id, sequence, timestamp_utc (when synchronized), uptime_ms, source, record_type, mode (test/production), firmware_version, config_version.
Sample every 30 seconds, and emit immediate events on transitions.
Thermostat sample fields: temperature_f, temperature_valid, setpoint_f, heat_demand, control_mode, requested_servo_us, requested_encoder_deg, reported_encoder_deg, encoder_valid, controller_link_state, controller_last_seen_ms, controller_fault.
Controller events: command_id, command_received_ms, servo_us_requested, servo_us_applied, encoder_raw, encoder_angle_deg, magnet_detected, weak_magnet, strong_magnet, movement_result, fault_code.
Events: boot, reboot_reason, wifi_connected/disconnected, espnow_connected/disconnected, mqtt_connected/disconnected, setpoint_changed, control_mode_changed, servo_move_requested, servo_move_completed, servo_move_failed, sensor_invalid, config_imported, config_rolled_back.
Missing readings must be null/invalid, not zero. Never use encoder=0 as a valid angle when magnet is missing.

## Export bundle
Single ZIP: manifest.json (schema, firmware versions, config version, UTC offset/sync status, capture period, test flag), thermostat.csv, controller_events.csv, events.csv, config_redacted.json, README.txt. Exclude Wi-Fi credentials, MQTT passwords, pairing secrets, tokens.
Rolling bounded storage with dropped-record counter; do not assume unlimited flash endurance. Download/export via thermostat web UI. Test-mode data must not become training baseline.

## Controller web interface
Separate manual test mode from thermostat control. Show live measured encoder angle, requested position, AS5600 magnet status, ESP-NOW status, and fault state on the same screen as manual movement controls. Buttons: Enter Servo Test Mode, Move/Test Position, Return to Thermostat Control. Return to thermostat control must be explicit; no uncommanded servo movement on startup or mode switch. Maintain mechanical travel and furnace safety limits independently of UI.

## Thermostat web interface
Dashboard: current temperature, setpoint, heat demand, controller online, actual angle, requested angle, active config version, logging state. Diagnostics: start/stop TEST capture, export bundle, dropped-record count, sensor and timestamp health. Configuration: preview, validate, import, revert last-known-good; never automatically apply ChatGPT suggestions.

## Tomorrow bench verification
1. Boot/reboot both boards; verify ESP-NOW reconnect and no unexpected servo motion.
2. Enter test mode, command several positions, compare requested and actual angles, return to thermostat control.
3. Disconnect controller; confirm stale telemetry and safe handling, then reconnect.
4. Record temperature and setpoint change, servo movements, encoder magnet absent/weak, time sync loss, Wi-Fi/MQTT loss.
5. Export bundle; check timestamps, sequence continuity, boot IDs, schema, missing values, event correlation and redaction.
6. Reboot; verify persistent settings and correct configuration version.
7. Discard TEST data; no changes to furnace control algorithm.

## Home Assistant enrichment (off-device)
Join exported thermostat data with Home Assistant furnace wattage/current, SwitchBot Hub 3 temperature, outdoor temperature/wind, and optional vibration. Keep sources and confidence explicit; do not use kids-room or keg temperatures for normal furnace control. Compare heating cycles under comparable weather and starting temperatures. Actual furnace response cannot be validated on the bench.

## Safety and acceptance
Controller rejects out-of-range commands, reports faults, and never autonomously learns or moves. Thermostat remains functional without HA, MQTT, or ChatGPT. Any future learned configuration requires human review, validation, versioning and rollback. Bench data is disposable.

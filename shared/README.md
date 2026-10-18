# Future shared thermostat protocol (not yet implemented)

The furnace controller and separate room display will eventually share a versioned application protocol over authenticated local Wi-Fi.

Proposed messages:
- display -> controller: temperature observation + source timestamp/quality; requested setpoint/mode, UI presence, calibration command only when authorized.
- controller -> display: current mode/setpoint, physical angle if verified, commanded PWM (distinct from actual position), freshness/fault state, heater activity, ETA to desired temperature.
- all commands require unique ID, acknowledgement/rejection reason and status propagation; no optimistic assumption that a knob actually moved.
- no remote control of level 0/OFF by generic heating APIs.
- controller persists schedule/calibration locally; HA/MQTT is optional.
- pairing/authentication and replay protection must be designed before production; bench PWM web UI is not this protocol.

**Not yet implemented.** See \`../docs/PROJECT_SPEC.md\`.

# Furnace activity monitoring: controller 0.7.4 / room panel 0.15.3

In the thermostat webpage, open **Furnace power**:

- **Monitor furnace power** defaults off. Enable it to poll the two HA sensors
  and classify their electrical stages. Disabling it stops their future polls
  and clears the observation session while other HA features continue.
- **Thermostat installed at cottage** defaults off. Leave it unchecked while
  testing on a bench/desk. This distinguishes `bench_monitor` from
  `onsite_monitor`; neither setting enables automatic thermal corrections.
- **Source report freshness limit** defaults to 180 seconds, adjustable from
  30 to 900 seconds. Match it to the plug's actual reporting cadence. A recent
  HTTP read is not proof the plug has recently reported.

Save applies immediately and persists in a separate NVS key. No reboot,
credential-profile migration or controller command is needed. Missing/corrupt
saved configuration defaults to monitoring disabled and bench testing.

## Inputs and inferred stages

The existing authenticated, TLS-verified HA client reads:

| Entity | Required unit | Use |
| --- | --- | --- |
| `sensor.furnace_power_power` | W | Electrical stage detection |
| `sensor.furnace_power_current` | A | Supporting read-only telemetry |

Current is not converted into watts using an assumed voltage or power factor.
Missing current does not block stage detection from valid watts. Neither
sensor exposes a switch action or a servo command.

The provisional bands come from the supplied October 27 power history:

| Stage | Entry rule |
| --- | --- |
| Standby | 0–2 W; remains standby below 5 W |
| Startup | 5–50 W |
| Likely heating | 115–160 W; remains in that stage down to 110 W |
| Likely blower cooldown | 75–105 W following observed likely heating; remains through 110 W |
| Unclassified load | 75–105 W without preceding likely heating, or above 160 W |
| Transition | Remaining gaps between bands |

Stages are inferences about equipment activity, not verified burner/flame
state. Two distinct source reports spanning at least 10 seconds must support
any stage change. Re-reading one cached HA state cannot confirm a stage.
A confirmed likely-heating → likely-cooldown → standby sequence increments
`completed_cycles`. Incomplete sequences and short excursions do not count.
Counts are since the current observation session, not persisted calibration.

`last_reported` is parsed as RFC3339, including fractional seconds and timezone
offsets. Unknown/unavailable, wrong units, negative/nonfinite values, missing
source timestamps, unsynchronized time, excessive report age, Wi-Fi loss,
auth failure or missing/stale watts yield Unknown rather than furnace-off.
An observation gap severs cycle context. Completed counts survive a data gap,
but restart or a configuration change starts a new observation session.

## Status and bench behavior

`/api/device/status` includes `furnace_power` with flags, `mode`, inferred
`activity`, `pending_activity`, `completed_cycles` and `adjustments_enabled`.
The two readings include entity ID, numeric value when available, unit, HA-read
age, `last_reported` and `source_fresh`. Logs report electrical-stage changes.
The webpage displays the stage, pending confirmation and observed cycle count.

With Installed unchecked, remote cottage power is never treated as evidence
that the desk/bench temperature is a furnace response. With Installed checked,
this release still observes only: `adjustments_enabled` stays false. Active
learning would additionally require validated colocated heating cycles and
explicit unoccupied eligibility. Unknown occupancy must suspend learning.
Kids-room/radiator and keg-fridge readings are not replacement control sensors.

Controller 0.7.4 continues holding the calibrated setpoint. Sunshine cannot
accumulate downward trim, and missing HA/plug data cannot prevent ordinary
local thermostat operation. These monitoring flags do not change Live/Service,
setpoint, Away, travel limits or saved dial/temperature estimates.

## Validation and local installation

Tests exercise the production state machine, timestamp parsing, hysteresis,
10-second source-report debounce, duplicate/out-of-order cache rejection,
freshness, inferred cooldown context, partial/full cycles and anomalous loads.
HA/backend fixtures cover opt-out polling, bench/onsite state, missing current,
invalid/stale source evidence, NVS failure/defaults, persistence and no reboot.
The webpage fixture checks flags, save/readback, activity and unavailable data.

Build and flash the room panel as 0.15.3. Keep controller 0.7.4 installed; an
older controller must also be updated to remove its temperature-only trim.
ESP-IDF builds and physical verification remain local. After flashing:

1. Open **Furnace power**, enable **Monitor furnace power**, leave
   **Thermostat installed at cottage** unchecked, and save.
2. Confirm watts/amps, a recent source timestamp, and inferred stages as reports
   arrive. Initial Unknown/pending confirmation is expected.
3. Verify settings survive a restart. Disabling monitoring must leave the rest
   of the thermostat and HA features working normally.

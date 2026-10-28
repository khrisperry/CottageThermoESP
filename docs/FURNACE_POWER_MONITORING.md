# Furnace electrical monitoring: controller 0.7.4 / room panel 0.15.2

The room panel reads these two sensor states using its existing authenticated,
TLS-verified Home Assistant connection:

| Entity | Required unit | Use |
| --- | --- | --- |
| `sensor.furnace_power_current` | A | Read-only electrical current |
| `sensor.furnace_power_power` | W | Read-only electrical power |

The thermostat webpage has a **Furnace power** section, and
`/api/device/status` includes a `furnace_power` snapshot. Readings include the
entity ID, unit, numeric value when available, time since the successful HA
read, and HA's `last_reported` timestamp when supplied. Missing, nonnumeric,
negative, nonfinite or wrong-unit values are unavailable. Reads expire after
30 seconds without success, and Wi-Fi loss/auth rejection invalidates them.
A recent HA read can still contain an old plug report; the two timestamps
have different meanings. Zero is a valid electrical reading, not proof that
the gas burner is off. The monitor never calls a plug switch service.

Electrical readings do not yet classify burner activity. Idle, ignition,
blower and cooldown values have not been measured on this installation. No
arbitrary watt/amp threshold or assumed voltage conversion is installed.
`activity` stays `unknown`, `mode` is `monitor_only`, and
`adjustments_enabled` is false. The original eleven dashboard mappings remain
unchanged; the two extra monitor entities are kept separately in the mapping.

## Controller behavior

Controller v0.7.4 removes temperature-only downward trim. A target still moves
to its calibrated dial setting under the existing travel, encoder, temperature
freshness and Live/Service checks. Sunshine, prolonged warmth and deck testing
cannot reduce that setting. Cooling later at night retains the original
calibrated target. Missing HA or plug data does not prevent ordinary local
thermostat operation. Saved temperature estimates are not rewritten.

This update does not enable automatic power-based corrections or persistent
calibration learning. While the room thermostat is on the user's deck rather
than at the cottage, its temperature is not a furnace-response measurement.
Even a genuine cottage electrical cycle must not teach a relationship to
that offsite temperature. Kids-room/radiator and keg-fridge readings are not
substitutes for the colocated primary room sensor.

## Evidence required for the next stage

Record watts/amps while the furnace is idle, starting, actually heating, and
cooling down. Note which phases have the burner versus only the blower active
and the plug's normal reporting interval. Those observations are needed for
thresholds, hysteresis, debounce and report-age limits. Then validate complete
cycles with the primary room sensor installed at the cottage. Active learning
must require explicit unoccupied confirmation; unknown occupancy suspends it.
Any future correction needs bounded, reversible behavior as the room cools,
rather than accumulating a permanent downward bias from sunshine.

## Validation and installation

Host tests cover an hour of sunshine above a 60F target followed by nighttime
cooling, with no trim, while deliberate target changes still move promptly.
The real HA client fixture tests the exact entity paths, numeric/unit checks,
missing/stale/unavailable data, read-only service rejection and auth/Wi-Fi
recovery. The webpage fixture checks numeric values, report time, unavailable
readings and the unknown activity label. No real HA endpoint was accessed.

Build and flash **both** firmware applications locally. Updating the panel
alone does not disable trim in an older controller. ESP-IDF builds and physical
plug/furnace verification remain pending; this workspace ran host fixtures.
After installation, check controller `/api/status` reports version 0.7.4 and
`heat_feedback.automatic_adjustments_enabled: false`, and verify room version
0.15.2 displays both entities. There is no new HA credential or token gate.

# Passive furnace temperature learning (review-only)

## Purpose and deployment

The room thermostat collects one synchronized observation every five minutes. It
reads its own corrected/raw probe, the controller's **read-only**
`/api/status` over LAN, and the thermostat's existing Home Assistant furnace
power and weather snapshots. No new control commands, protocol fields, setpoint
changes, PWM changes, controller settings, or calibration writes are made.

Only the **room thermostat** firmware needs updating for this phase. The
controller stays on the working firmware. Automatic model adjustments remain
disabled, and the existing 50 F Away/Freeze logic is unchanged.

## Read-only endpoints on the thermostat

- `GET /api/learning` — collector status, pending/eligible sample counts,
  preliminary quarter-step averages and reasons for limitations.
- `GET /api/learning.csv` — up to 7 days of 5-minute raw samples, or 24 hours
  when PSRAM allocation is unavailable. Download before reboot: the detailed
  CSV ring is held in RAM.
- `GET /api/device/status` contains a compact `learning` status object.
- Thermostat `/settings` > Learning provides an overview and CSV download.

CSV columns include timestamp, raw/corrected room temperature, target,
**commanded** dial level, AS5600 encoder angle and validity, actual Live/mode,
furnace stage and electrical watts/amps, outdoor temperature/cloud cover,
freshness flags, furnace monitoring enabled/installed settings and review eligibility.
Raw watt/current values are recorded when available, even if monitoring is not
marked installed; that setting only gates the provisional summaries. An empty numeric field indicates
missing/unverified data. Missing LAN details are **not** inferred from ESP-NOW.

The source `/api/status` on the controller reports
`position_measured=false`. AS5600 feedback verifies the magnet angle, **not**
necessarily the gas knob's actual position. All dial values in learning output
are labeled **commanded**, never confirmed physical positions.

## Eligibility for provisional averages

Samples are logged regardless of eligibility; the *candidate* averages require
a fresh primary temperature, matching read-only controller identity, Live active
without fault/verification, a synchronized position with a valid encoder angle,
onsite enabled and fresh HA furnace electrical telemetry, fresh outside
temperature, no commanded dial or target change for 90 minutes, and a corrected
temperature spread <=1.5 F across at least six consecutive 5-minute samples.

Normal Live control uses fractional positions (for example 2.727 for a 70 F
setpoint). Provisional averages use quarter-step bins 1.0, 1.25, ... 6.0;
the reported mean commanded level preserves the actual values in each bin.
The raw CSV always retains exact fractional commands, and the user should
verify the physical knob independently. Bin widths introduce rounding,
so these summaries are indicative, not a replacement for CSV analysis.
Averages are **not** definitive equilibrium temperatures: burner electrical
stages are inferred, solar gain and building thermal lag can still bias them,
and all changes require manual review.

Model schema 2 adds quarter-step bins and actual mean dial position; any prior
schema-1 provisional summary is reset during this upgrade (raw history remains
RAM-only). Provisional aggregate bins are saved to a separate NVS namespace at most once
every six hours while eligible samples are accumulating. This minimizes flash
wear; up to six hours of unsaved provisional bins and the detailed CSV ring can
be lost on power loss. Existing credential and controller calibration NVS
blobs are untouched. Neither raw history nor accumulated bins modify control.

## Manual field test

1. Leave the current calibrated dial map intact and operate the furnace in
   normal Live mode. Confirm the thermostat's furnace power monitor is enabled
   and marked installed, the SHT45 reading/offset are accurate, and
   `weather.forecast_home` is fresh.
2. Run at a fixed setting (first 2.5, then 3) for several heating/cooling
   cycles. Download CSV and note actual physical dial position independently.
3. Correlate timestamps with HA furnace stages and local weather. Reject
   temperature points during startup, solar gain, target changes, sensor
   loss, manual operation, controller faults, and link outages.
4. Compare candidate averages to 2.5 -> 68.5 F and 3 -> 73 F field
   observations. **Do not** automatically change the controller dial map.
5. Confirm controller feedback and freezing behavior under direct observation
   before extending the automatic operating range or enabling any future trim.

Until updated room firmware is built, flashed and verified on device, the
collector is not running. No background telemetry is being gathered by GitHub.

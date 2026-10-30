# Controller v0.7.3 restart and link recovery

The room panel v0.15.1 also has **System → Restart thermostat** on its own
webpage. It sends `POST /api/settings` with
`{"section":"restart","confirm":"restart_thermostat"}`, uses the existing
delayed reboot, and leaves NVS settings intact. Same-origin checks apply and an
active OTA upload or pending restart returns 409. The page disables restart
and upload controls while restarting and waits for a changed boot ID before
reporting the thermostat online. This button restarts only the room panel;
the controller's saved operating mode is unaffected.

After pulling, build/flash the panel with
`.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21` from the repository root,
or upload the built application `.bin` through its existing System page.

The controller webpage has a **Restart controller** button beside the operating
mode controls. Its confirmed `POST /api/restart` request contains
`{"confirm":"restart_controller"}`. PWM stops and further movement requests
are blocked while a delayed restart allows the HTTP response to finish.
Restart does not call Stop/Service or rewrite the saved mode, target, Away
setting, calibration, network profile or pairing. On boot, the existing Live
checks still require valid calibration, stable calibrated AS5600 position and
a fresh primary thermostat sample. Bench Go is temporary and is not restored.

While persistent Live is selected and no latched Live fault exists, a full
30-minute interval without an accepted primary thermostat sample schedules the
same restart. Both HTTP and encrypted ESP-NOW samples reset the interval.
Browser requests, status polls and rejected/stale sensor reports do not. The
timer starts even if the controller never establishes its first connection.
Service/Calibration pauses the timer; selecting Live begins a new full window.
An outage that continues across boots can cause one restart per 30-minute
window. A restart is a recovery attempt, not proof that the underlying Wi-Fi
or HTTP failure has been corrected.

`GET /api/status` includes `restart_pending` and a `link_recovery` object with
`enabled`, `timeout_ms`, `remaining_ms` when enabled, and
`last_sample_age_ms` after the first accepted sample this boot. The controller
page displays the recovery countdown. Active Live at its mapped dial position
can correctly leave PWM off; the page now labels that state **Monitoring** /
**Live active · no movement requested** instead of implying Live has stopped.

## Validation

Host tests exercise the production recovery policy and restart handler with a
fake monotonic clock and fake task/hardware boundaries: never connected,
30-minute boundary, accepted sample reset, long uptime, Service exemption,
confirmation, deferred restart, preserved Live/Service, PWM-off and task creation
failure. The browser test exercises cancellation, one restart request without
Stop/Service, pending/offline controls and Live monitoring with PWM off.

Build and flash only the controller from an ESP-IDF 6.0.1 terminal:

```powershell
git pull --ff-only
cd controller
idf.py -B build-supermini -D SDKCONFIG=sdkconfig.supermini -D SDKCONFIG_DEFAULTS=sdkconfig.supermini.defaults build flash monitor
```

Hardware checks: restart from Live and verify `selected=yes`, then
`Live restored` after fresh sensor/encoder checks. Change the target and verify
the physical response. Restart from Service and verify it stays in Service.
With Live selected, interrupt thermostat reports and watch the countdown;
restoring reports before expiry must reset it. A full 30-minute interruption
must restart the controller with the saved Live selection and PWM initially
off. Explicit encoder/PWM faults must remain in Service across a restart.

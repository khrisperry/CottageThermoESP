# Weather observations and native outside temperature: room panel 0.15.4

The thermostat reads **`weather.forecast_home`** through its existing
TLS-verified HA connection. No additional credential is needed. This is
current-weather monitoring; it does not request a forecast or call a weather
service. HA documents current entity attributes separately from the
`weather.get_forecasts` action:
https://www.home-assistant.io/integrations/weather/

## Native Climate screen

The left column now shows the target, **Current** room temperature, then
**Outside** temperature. The HEAT/title/temperature group is shifted upward
slightly to fit the extra line. The blue/orange target dial and all four
bottom buttons keep their original positions and input behavior.

Outside uses the cached weather snapshot, converted to Fahrenheit. It never
substitutes for the primary room sensor or changes the thermostat target.
Missing/invalid weather, Wi-Fi loss, expired worker data, or stale source data
shows `Outside -- F`. The LVGL refresh callback performs no network request.

## Webpage and API

The **Weather** page and Overview show current conditions and outdoor
temperature. The detailed page also shows apparent temperature, humidity,
cloud coverage, wind speed and gust speed when the provider supplies them.
`/api/device/status.weather` exposes the same sanitized snapshot, with source
timestamp, HA read age, HTTP status and availability. Temperature is normalized
to F and wind to mph using the provider's declared units. Missing or invalid
optional fields are omitted, rather than fabricated as zero. Unknown units
are not guessed. Cloud coverage of zero remains a valid reading.

Successful weather reads are spaced five minutes apart, failed reads retry
after 30 seconds, and Wi-Fi recovery requests a fresh read. Weather monitoring
continues independently of the furnace-power enable flag and controller link.
A missing/failed weather entity does not stop other HA entity polling. Shared
HA authentication failure still invalidates all HA observations normally.

A snapshot expires after 15 minutes without a successful read. Source time
uses `last_reported`, falling back to `last_updated`, with a two-hour source
age limit and the existing timezone-aware timestamp parser. Stale/missing
source time is disclosed as unavailable. No arbitrary provider attributes,
tokens or URLs are copied into status responses.

Weather is observational in both bench and onsite modes. Sunny conditions or
low cloud cover are context, not a measurement of heat entering the room.
Automatic thermal adjustments and calibration learning remain disabled.

## Validation and local build

The weather parser tests cover current conditions, identity, C/F and wind-unit
conversion, missing/invalid fields and timestamp fallback. HA tests exercise
the exact entity path, cache interval, stale weather, optional 404 failures,
credential filtering and independence from furnace monitoring. Webpage tests
check weather values and unavailability. The focused native LVGL test renders
the actual 480x480 Climate screen, checks room/outside/target bounds, weather
loss, Wi-Fi loss/recovery, worker freshness and that no command is sent.

The native test is `climate_weather_test` in `tests/native/CMakeLists.txt`, using
the pinned LVGL 9.2.0 dependency and mocked hardware/network boundaries. Its
render was visually inspected. These checks do not emulate the physical LCD.

Build/flash **the thermostat only**, as 0.15.4. Keep controller 0.7.4. From the
repo root in the local ESP-IDF terminal:

```powershell
git pull --ff-only
.\tools\build-room-4b.ps1 -Flash -Monitor -Port COM21
```

Verify the actual Climate screen and the Weather webpage after flashing. The
workspace ran host tests and LVGL rendering; the ESP-IDF build and physical
display verification remain local.

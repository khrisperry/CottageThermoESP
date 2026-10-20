# Native LVGL host checks

Run after the 4B firmware build has fetched its pinned managed components. Uses a host C compiler and CMake (Linux tested), real LVGL 9.2.0 and the production `room_display.c`. Network and RTOS calls are replaced with local fixtures; no HA endpoint or physical device is accessed.

```sh
cmake -S tests/native -B tests/native/build
cmake --build tests/native/build -j 6
cd tests/native/build
./native_ui_test
./direct_ha_test
./empty_ha_test
```

The program renders Climate/Home/Laundry/Camera to PPM files for layout inspection, injects a real LVGL pointer swipe, and checks commands, stale controls, unknown outcomes, expired requests, target-only climate changes, I2C pin protection and idle return. It does not emulate the RGB LCD, touch controller, Wi-Fi, PSRAM or JPEG decoder. Those require the first physical 4B test described in `touchscreen/README.md`.

The direct client tests exercise production `room_ha.c` against a fake HTTP transport, including the bearer header and actual HA state/service/camera paths. The empty-token target verifies setup mode performs no requests. Neither test uses a real HA credential or network connection.

Version 0.6.0 adds `wifi_portal_test` for the production pure-C SSID/password limits and DNS A/AAAA/bounds handling, and a native Wi-Fi button check. These new C checks have not been executed in the unavailable coding workspace. Run `./wifi_portal_test` after rebuilding. The portal JavaScript was checked with mocked DOM/network responses; real captive detection, DHCP, AP/STA switching, NVS retention and reconnect behavior require the physical board.

Version 0.6.2 adds `keepalive_test`, exercising the production transport with a fake IDF HTTP client: eleven polls use one handle, POST/GET body reset, failed requests drop the session without replaying a POST, response bounds/completion, redirect/auth behavior and origin/time guards. The direct HA fixture also checks I/O serialization, command priority and retained state after transient errors. These new/updated C checks have not been run in the unavailable coding workspace.

```sh
./keepalive_test
./direct_ha_test
./empty_ha_test
```

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

Version 0.6.3 corrects initial GET setup: IDF 6.0.1 returns `ESP_ERR_NOT_FOUND` when `set_post_field(NULL, 0)` deletes a missing Content-Type. The transport now sets Content-Type before resetting the body. The fixture models the actual missing-header behavior and checks GET → POST → GET. HA startup, request failures and successful polling now produce credential-free diagnostics. Source/API review only; C compilation/tests remain unavailable in the coding workspace.

Version 0.7.0 restyles the four production LVGL pages and adds `camera_size_test`, which exercises the bounded JPEG marker parser and 1/2, 1/4, 1/8 downscaling selection, including 1080p/4K, portrait, progressive and malformed inputs. The native UI target still renders the actual screens and checks swipe/control behavior. These C targets and firmware have not been compiled or executed for this revision because the execution workspace is unavailable.

```sh
./camera_size_test
./native_ui_test
```

v0.8.1 adds `feedback_test` for cached versus fresh optimistic acknowledgements, five-second rollback and fresh single-shot laundry completion transitions. The native fixture checks independent per-appliance notification toggles and preserves optimistic state across an old cached read. The direct HA fixture supplies a no-op sound sink. These C tests have not been run in the unavailable execution workspace. Run `./feedback_test`, `./native_ui_test`, `./direct_ha_test` and `./empty_ha_test` locally after building.

v0.8.2 adds `audio_test`, which compiles the production audio module with isolated codec, FreeRTOS and NVS stubs. It checks stereo PCM channel equality and peak level, 16 kHz/16-bit format, volume, amp warm-up and drain delays, failure muting, explicit Sound testing while notifications are off, and one-cycle preference consumption/persistence, including a full queue.

This test was compiled and passed with host GCC in the coding workspace. It does not prove speaker wiring, hardware codec output or audibility.

```sh
gcc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -I tests/native/audio_stubs -I touchscreen/main tests/native/test_audio.c -lm -o /tmp/cottage-audio-test
/tmp/cottage-audio-test
```

v0.8.3 changes expected codec volume to 100 and PCM peak to full-scale 32767. Feedback tests preserve a running cycle for auto-off across unavailable/stale samples while suppressing a stale sound. The direct HA fixture checks a missing washer read followed by Not In Use clears its toggle without audio, and a normal fresh completion invokes the sound only once. These revised tests have not been executed because the workspace is unavailable again.

Version 0.9.0 adds camera_scale_test for aspect-preserving Fit/Fill and upscaling; the native UI fixture checks Settings, saved-volume calls, Wi-Fi setup and Fit/Fill selection. The production audio fixture now checks volume persistence, restart loading, bounds and mute. These revised C checks have not been executed because the execution workspace is unavailable. Run ./camera_scale_test, ./audio_test and ./native_ui_test after rebuilding locally.

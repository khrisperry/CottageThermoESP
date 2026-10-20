# Native LVGL host checks

Run after the 4B firmware build has fetched its pinned managed components. Uses a host C compiler and CMake (Linux tested), real LVGL 9.2.0 and the production `room_display.c`. Network and RTOS calls are replaced with local fixtures; no HA endpoint or physical device is accessed.

```sh
cmake -S tests/native -B tests/native/build
cmake --build tests/native/build -j 6
cd tests/native/build
./native_ui_test
```

The program renders Climate/Home/Laundry/Camera to PPM files for layout inspection, injects a real LVGL pointer swipe, and checks commands, stale controls, unknown outcomes, expired requests, target-only climate changes, I2C pin protection and idle return. It does not emulate the RGB LCD, touch controller, Wi-Fi, PSRAM or JPEG decoder. Those require the first physical 4B test described in `touchscreen/README.md`.

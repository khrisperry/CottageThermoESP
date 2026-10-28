cmake_minimum_required(VERSION 3.16)
project(thermostat_host_tests C)
enable_testing()
add_executable(test_thermo test_thermo.c ../shared/thermo/thermo.c)
target_include_directories(test_thermo PRIVATE ../shared/thermo)
if(NOT MSVC)
    target_compile_options(test_thermo PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test_thermo PRIVATE m)
endif()
add_test(NAME thermostat COMMAND test_thermo)

add_executable(test_link test_link.c)
if(NOT MSVC)
    target_compile_options(test_link PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test_link PRIVATE m)
endif()
add_test(NAME transport_protocol COMMAND test_link)

add_executable(test_room_link_policy test_room_link_policy.c)
target_include_directories(test_room_link_policy PRIVATE
    ../touchscreen/main ../shared/thermo_link)
if(NOT MSVC)
    target_compile_options(test_room_link_policy PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME room_link_policy COMMAND test_room_link_policy)

# Pure host-side checks: actual 480x480 layout geometry and light-tile
# action policy. No LVGL or ESP-IDF environment needed.
add_executable(test_room_home_layout test_room_home_layout.c)
target_include_directories(test_room_home_layout PRIVATE ../touchscreen/main)
if(NOT MSVC)
    target_compile_options(test_room_home_layout PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME room_home_layout COMMAND test_room_home_layout)

# Digital clock/idle policy is display-only and can be checked without LVGL,
# hardware, network access or ESP-IDF.
add_executable(test_room_clock test_room_clock.c)
target_include_directories(test_room_clock PRIVATE ../touchscreen/main)
if(NOT MSVC)
    target_compile_options(test_room_clock PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME room_clock COMMAND test_room_clock)

# The primary sensor's Fahrenheit calibration is pure logic; use the same
# header in ESP-IDF and local host tests without needing LVGL or NVS.
add_executable(test_room_temp_cal test_room_temp_cal.c)
target_include_directories(test_room_temp_cal PRIVATE ../touchscreen/main)
if(NOT MSVC)
    target_compile_options(test_room_temp_cal PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test_room_temp_cal PRIVATE m)
endif()
add_test(NAME room_temp_cal COMMAND test_room_temp_cal)

add_executable(test_network test_network.c)
if(NOT MSVC)
    target_compile_options(test_network PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME network_profile COMMAND test_network)

add_executable(test_wifi_retry test_wifi_retry.c)
if(NOT MSVC)
    target_compile_options(test_wifi_retry PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME wifi_retry COMMAND test_wifi_retry)

add_executable(test_controller_wifi_retry test_controller_wifi_retry.c)
if(NOT MSVC)
    target_compile_options(test_controller_wifi_retry PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME controller_wifi_retry COMMAND test_controller_wifi_retry)

# Compile the *exact* controller LED status policy on the local host.
# This test does not drive RMT hardware or flash a device.
add_executable(test_controller_status_led test_controller_status_led.c)
target_include_directories(test_controller_status_led PRIVATE ../controller/main)
if(NOT MSVC)
    target_compile_options(test_controller_status_led PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME controller_status_led COMMAND test_controller_status_led)

add_executable(test_controller_encoder test_controller_encoder.c ../controller/main/controller_encoder.c)
target_include_directories(test_controller_encoder PRIVATE encoder_stubs)
if(NOT MSVC)
    target_compile_options(test_controller_encoder PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME controller_encoder COMMAND test_controller_encoder)

# Compile the EXACT production PWM update implementation with a fake ESP-IDF
# LEDC interface. The fake header deliberately excludes fade-service methods;
# accidental reuse of the ESP-IDF 6.0.1 fade-dependent combined call fails CI.
add_executable(test_controller_pwm_duty test_controller_pwm_duty.c ../controller/main/controller_pwm_duty.c)
target_include_directories(test_controller_pwm_duty PRIVATE ledc_stubs ../controller/main)
if(NOT MSVC)
    target_compile_options(test_controller_pwm_duty PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME controller_pwm_duty COMMAND test_controller_pwm_duty)

add_executable(test_controller_setup test_controller_setup.c ../controller/main/controller_setup.c ../shared/thermo/thermo.c)
target_include_directories(test_controller_setup PRIVATE ../controller/main ../shared/thermo)
if(NOT MSVC)
    target_compile_options(test_controller_setup PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test_controller_setup PRIVATE m)
endif()
add_test(NAME controller_setup COMMAND test_controller_setup)

add_executable(test_controller_live test_controller_live.c ../controller/main/controller_live.c
    ../controller/main/controller_setup.c ../shared/thermo/thermo.c)
target_include_directories(test_controller_live PRIVATE ../controller/main ../shared/thermo)
if(NOT MSVC)
    target_compile_options(test_controller_live PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(test_controller_live PRIVATE m)
endif()
add_test(NAME controller_live COMMAND test_controller_live)

# Pass -DCJSON_SOURCE_DIR=<checkout of DaveGamble/cJSON> to exercise the actual
# controller HTTP workflow with fake hardware/NVS boundaries on a host.
if(CJSON_SOURCE_DIR)
    add_executable(test_controller_recovery test_controller_recovery.c "${CJSON_SOURCE_DIR}/cJSON.c")
    target_include_directories(test_controller_recovery PRIVATE ../controller/main "${CJSON_SOURCE_DIR}")
    if(NOT MSVC)
        target_compile_options(test_controller_recovery PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME controller_recovery COMMAND test_controller_recovery)

    add_executable(test_controller_setup_api test_controller_setup_api.c ../controller/main/controller_setup.c ../controller/main/controller_live.c ../shared/thermo/thermo.c "${CJSON_SOURCE_DIR}/cJSON.c")
    target_include_directories(test_controller_setup_api PRIVATE ../controller/main ../shared/thermo "${CJSON_SOURCE_DIR}")
    if(NOT MSVC)
        target_compile_options(test_controller_setup_api PRIVATE -Wall -Wextra -Werror)
        target_link_libraries(test_controller_setup_api PRIVATE m)
    endif()
    add_test(NAME controller_setup_api COMMAND test_controller_setup_api)

    # A previously healthy ESP-NOW link could never return to HTTP after
    # a temporary Wi-Fi isolation: /api/status grew beyond an 1800-byte
    # stack buffer, so discovery logged ESP_FAIL with HTTP 200. Compile
    # the ACTUAL probe with mocked HTTP and verify recovery without reboot.
    add_executable(test_room_discovery_probe test_room_discovery_probe.c
        ../touchscreen/main/room_discovery_probe.c "${CJSON_SOURCE_DIR}/cJSON.c")
    target_include_directories(test_room_discovery_probe PRIVATE
        discovery_stubs ../touchscreen/main "${CJSON_SOURCE_DIR}")
    if(NOT MSVC)
        target_compile_options(test_room_discovery_probe PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME room_discovery_probe COMMAND test_room_discovery_probe)

    add_executable(test_room_mqtt_preset test_room_mqtt_preset.c "${CJSON_SOURCE_DIR}/cJSON.c")
    target_include_directories(test_room_mqtt_preset PRIVATE ../touchscreen/main "${CJSON_SOURCE_DIR}")
    if(NOT MSVC)
        target_compile_options(test_room_mqtt_preset PRIVATE -Wall -Wextra -Werror)
    endif()
    add_test(NAME room_mqtt_preset COMMAND test_room_mqtt_preset)
endif()

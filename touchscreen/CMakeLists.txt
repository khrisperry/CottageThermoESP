cmake_minimum_required(VERSION 3.16)
# Build main and its declared dependencies; omit unused framework drivers.
set(COMPONENTS main)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared/thermo_link")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cottage_thermo_room_4b VERSION 0.14.7)


if(NOT CONFIG_ROOM_NATIVE_DISPLAY OR NOT CONFIG_SPIRAM_MODE_OCT OR NOT CONFIG_ESPTOOLPY_FLASHSIZE_16MB)
    message(FATAL_ERROR "This application/OTA identity is exclusively for the native Waveshare 4B with 16 MB flash and octal PSRAM")
endif()
idf_component_get_property(ROOM_LVGL_LIBRARY lvgl__lvgl COMPONENT_LIB)
target_include_directories(${ROOM_LVGL_LIBRARY} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/main")
target_compile_definitions(${ROOM_LVGL_LIBRARY} PRIVATE LV_MEM_POOL_INCLUDE="room_lvgl_memory.h" LV_MEM_POOL_ALLOC=room_lvgl_pool_alloc)

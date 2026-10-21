cmake_minimum_required(VERSION 3.16)
# Build main and its declared dependencies; omit unused framework drivers.
set(COMPONENTS main)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared/thermo" "${CMAKE_CURRENT_LIST_DIR}/../shared/thermo_link")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cottage_thermo_controller VERSION 0.5.6)

# Reject incompatible saved configuration rather than silently using the wrong flash size.
if(CONFIG_THERMO_SUPERMINI)
    if(NOT CONFIG_ESPTOOLPY_FLASHSIZE_4MB OR CONFIG_SPIRAM OR CONFIG_THERMO_LIVE_BENCH)
        message(FATAL_ERROR "Super Mini bench profile requires 4 MB flash, PSRAM disabled, and simulated automatic output. Check sdkconfig.supermini.")
    endif()
endif()

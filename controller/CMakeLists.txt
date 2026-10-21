cmake_minimum_required(VERSION 3.16)
# Build main and its declared dependencies; omit unused framework drivers.
set(COMPONENTS main)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../shared/thermo" "${CMAKE_CURRENT_LIST_DIR}/../shared/thermo_link")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cottage_thermo_controller VERSION 0.5.3)

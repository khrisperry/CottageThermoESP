cmake_minimum_required(VERSION 3.16)
# Build main and its declared dependencies; omit unused framework drivers.
set(COMPONENTS main)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cottage_thermo_room VERSION 0.4.0)

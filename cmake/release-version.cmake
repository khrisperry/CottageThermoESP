# Both applications take their release identity from the repository VERSION.
set(COTTAGE_VERSION_FILE "${CMAKE_CURRENT_LIST_DIR}/../VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${COTTAGE_VERSION_FILE}")
file(STRINGS "${COTTAGE_VERSION_FILE}" COTTAGE_THERMO_VERSION LIMIT_COUNT 1)
if(NOT COTTAGE_THERMO_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must contain a numeric major.minor.patch release")
endif()
set(PROJECT_VER "${COTTAGE_THERMO_VERSION}")

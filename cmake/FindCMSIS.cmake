####
# FindCMSIS.cmake:
#
# Finds CMSIS headers (ARM Core and Atmel Device) from the Arduino installation.
# Sets: CMSIS_FOUND, CMSIS_CORE_INCLUDE_DIR, CMSIS_DEVICE_INCLUDE_DIR, CMSIS_INCLUDE_DIRS
# Provides: CMSIS::Headers imported target
####

cmake_minimum_required(VERSION 3.16)

if(TARGET CMSIS::Headers)
    return()
endif()

set(CMSIS_FOUND FALSE)

# Strategy 1: Extract from arduino-cli JSON if available
if(DEFINED ARDUINO_WRAPPER_JSON_OUTPUT AND EXISTS "${ARDUINO_WRAPPER_JSON_OUTPUT}")
    file(READ "${ARDUINO_JSON_CONTENT}" "${ARDUINO_WRAPPER_JSON_OUTPUT}")
    string(JSON INCLUDES_TYPE ERROR_VARIABLE JSON_ERROR TYPE "${ARDUINO_JSON_CONTENT}" includes CXX)

    if(NOT JSON_ERROR AND INCLUDES_TYPE STREQUAL "ARRAY")
        string(JSON INCLUDES_LENGTH LENGTH "${ARDUINO_JSON_CONTENT}" includes CXX)
        math(EXPR INCLUDES_MAX "${INCLUDES_LENGTH} - 1")

        foreach(INDEX RANGE ${INCLUDES_MAX})
            string(JSON INCLUDE_PATH GET "${ARDUINO_JSON_CONTENT}" includes CXX ${INDEX})

            if(INCLUDE_PATH MATCHES ".*/CMSIS/[^/]+/CMSIS/Core/Include/?$" AND NOT CMSIS_CORE_INCLUDE_DIR)
                set(CMSIS_CORE_INCLUDE_DIR "${INCLUDE_PATH}")
            endif()

            if(INCLUDE_PATH MATCHES ".*/CMSIS-Atmel/[^/]+/CMSIS/Device/ATMEL/?$" AND NOT CMSIS_DEVICE_INCLUDE_DIR)
                set(CMSIS_DEVICE_INCLUDE_DIR "${INCLUDE_PATH}")
            endif()
        endforeach()
    endif()
endif()

# Strategy 2: Search common Arduino installation locations
if(NOT CMSIS_CORE_INCLUDE_DIR OR NOT CMSIS_DEVICE_INCLUDE_DIR)
    set(SEARCH_PATHS
        "$ENV{HOME}/.arduino15/packages/adafruit/tools"
        "$ENV{HOME}/.arduino15/packages/arduino/tools"
        "/usr/share/arduino/hardware/tools"
    )

    if(DEFINED ARDUINO_USER_DIRECTORY)
        list(INSERT SEARCH_PATHS 0 "${ARDUINO_USER_DIRECTORY}/packages/adafruit/tools")
    endif()

    if(NOT CMSIS_CORE_INCLUDE_DIR)
        find_path(CMSIS_CORE_INCLUDE_DIR
            NAMES core_cm0plus.h core_cm4.h
            PATHS ${SEARCH_PATHS}
            PATH_SUFFIXES CMSIS/5.4.0/CMSIS/Core/Include CMSIS/4.5.0/CMSIS/Include
            NO_DEFAULT_PATH
        )
    endif()

    if(NOT CMSIS_DEVICE_INCLUDE_DIR)
        find_path(CMSIS_DEVICE_INCLUDE_DIR
            NAMES samd.h sam.h
            PATHS ${SEARCH_PATHS}
            PATH_SUFFIXES CMSIS-Atmel/1.2.2/CMSIS/Device/ATMEL CMSIS-Atmel/1.2.0/CMSIS/Device/ATMEL
            NO_DEFAULT_PATH
        )
    endif()
endif()

# Validate and create target
if(CMSIS_CORE_INCLUDE_DIR AND CMSIS_DEVICE_INCLUDE_DIR)
    set(CMSIS_FOUND TRUE)
    set(CMSIS_INCLUDE_DIRS "${CMSIS_CORE_INCLUDE_DIR}" "${CMSIS_DEVICE_INCLUDE_DIR}")

    add_library(CMSIS::Headers INTERFACE IMPORTED GLOBAL)
    set_target_properties(CMSIS::Headers PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMSIS_INCLUDE_DIRS}"
    )

    if(NOT FindCMSIS_FIND_QUIETLY)
        message(STATUS "Found CMSIS Core: ${CMSIS_CORE_INCLUDE_DIR}")
        message(STATUS "Found CMSIS Device: ${CMSIS_DEVICE_INCLUDE_DIR}")
    endif()
else()
    if(FindCMSIS_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find CMSIS headers. Install via: arduino-cli core install adafruit:samd")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CMSIS
    REQUIRED_VARS CMSIS_CORE_INCLUDE_DIR CMSIS_DEVICE_INCLUDE_DIR
)

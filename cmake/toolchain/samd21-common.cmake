####
# samd21-common.cmake:
#
# Common toolchain setup for building F prime for the SAMD21 bare-metal platform.
# This file should be included by specific toolchain files (qtpy.cmake, microchip_curiosity.cmake)
# after setting board-specific linker script path.
#
# No Arduino-CLI invocation - uses hardcoded toolchain paths and flags.
####

# System setup for SAMD21 M0
set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_PROCESSOR "samd21")
set(CMAKE_CROSSCOMPILING 1)
set(FPRIME_PLATFORM "Samd21")
set(FPRIME_USE_BAREMETAL_SCHEDULER ON)

# Include bare-metal toolchain (no Arduino-CLI invocation)
include("${CMAKE_CURRENT_LIST_DIR}/samd21-bare-toolchain.cmake")

# Additional F' specific flags
set(SAMD21_COMMON_FLAGS "\
    -D__SAMD21__\
    -DNDEBUG \
    -Wall \
    -Wextra \
    -Wno-conversion \
    -Wdouble-promotion \
    -Wshadow \
    -Wno-unused-parameter \
    -Wno-vla \
    -Wno-expansion-to-defined \
    -Wno-type-limits \
")

# Add LTO if enabled
if (SAMD21_LTO)
    set(SAMD21_COMMON_FLAGS "${SAMD21_COMMON_FLAGS} -flto")
endif()

if (SAMD21_MTB)
    set(SAMD21_COMMON_FLAGS "${SAMD21_COMMON_FLAGS} -DMICRO_TRACE_BUFFER=256")
endif()

# Append to existing flags from bare-toolchain
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} ${SAMD21_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} ${SAMD21_COMMON_FLAGS}")

if (SAMD21_LTO)
    set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} -flto")

    # Get LTO plugin for archive operations
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} --print-file-name=liblto_plugin.so
        OUTPUT_VARIABLE LTO_PLUGIN
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    message(STATUS "[samd21] Detected LTO plugin: ${LTO_PLUGIN}")

    set(CMAKE_C_ARCHIVE_FINISH "<CMAKE_RANLIB> --plugin=${LTO_PLUGIN} <TARGET>")
    set(CMAKE_CXX_ARCHIVE_FINISH "<CMAKE_RANLIB> --plugin=${LTO_PLUGIN} <TARGET>")
    set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> rcs --plugin=${LTO_PLUGIN} <TARGET> <OBJECTS>")
    set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> rcs --plugin=${LTO_PLUGIN} <TARGET> <OBJECTS>")
endif()

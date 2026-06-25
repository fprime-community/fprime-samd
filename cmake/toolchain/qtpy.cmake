####
# qtpy.cmake:
#
# Toolchain file for SAMD21 QTPy (Adafruit QT Py M0) without USB support.
# This provides the smallest binary size by disabling the USB stack.
#
# Hardware: SAMD21E18A (256KB flash, 32KB RAM)
####

# IMPORTANT: Set device define BEFORE including toolchain
# This is required for <sam.h> to include the correct device header
add_compile_definitions(__SAMD21E18A__)

# Variant directory (relative to this toolchain file)
get_filename_component(QTPY_VARIANT_DIR "${CMAKE_CURRENT_LIST_DIR}/samd21/qtpy_m0" ABSOLUTE)

# Linker script
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} \
    -T${QTPY_VARIANT_DIR}/linker_scripts/flash_with_bootloader.ld \
    -L${QTPY_VARIANT_DIR}/linker_scripts \
")

# Include common SAMD21 toolchain setup
include("${CMAKE_CURRENT_LIST_DIR}/samd21-common.cmake")

# Board-specific defines (after toolchain, so they append to CMAKE_C/CXX_FLAGS_INIT)
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} \
    -DCRYSTALLESS \
    -DADAFRUIT_QTPY_M0 \
    -DARM_MATH_CM0PLUS \
    -DF_CPU=48000000L \
    -DUSE_NOUSB \
")

set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} \
    -DCRYSTALLESS \
    -DADAFRUIT_QTPY_M0 \
    -DARM_MATH_CM0PLUS \
    -DF_CPU=48000000L \
    -DUSE_NOUSB \
")

# Include variant headers (must be BEFORE CMSIS for sam.h to work)
include_directories(BEFORE "${QTPY_VARIANT_DIR}")

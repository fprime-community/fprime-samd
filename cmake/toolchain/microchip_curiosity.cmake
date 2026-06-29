####
# microchip_curiosity.cmake:
#
# Toolchain file for SAMD21 Curiosity Nano (SAMD21G17D)
# Hardware: SAMD21G17D (128KB flash, 16KB RAM)
# USB stack disabled - USB is connected to debugger only, not MCU
####

# IMPORTANT: Set device define BEFORE including toolchain
# This is required for <sam.h> to include the correct device header
add_compile_definitions(
    __SAMD21G17D__
    __SAMD21G17A__
)

# Enable LTO for this board (has limited flash)
set(SAMD21_LTO OFF)

# Set the board type
set(BOARD_TYPE "SAMD21_CURIOSITY_NANO")

# Variant directory (relative to this toolchain file)
get_filename_component(CURIOSITY_VARIANT_DIR "${CMAKE_CURRENT_LIST_DIR}/samd21/curiosity_nano" ABSOLUTE)

# Set linker script path (will be used after common toolchain is included)
set(SAMD21_LINKER_SCRIPT "${CURIOSITY_VARIANT_DIR}/linker_scripts/flash_without_bootloader.ld")

# Include common samd21 toolchain setup
include("${CMAKE_CURRENT_LIST_DIR}/samd21-common.cmake")

# Add linker script AFTER common toolchain (so it doesn't get overwritten)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} \
    -T${SAMD21_LINKER_SCRIPT} \
    -L${CURIOSITY_VARIANT_DIR}/linker_scripts \
")

# Board-specific defines (after toolchain, so they append to CMAKE_C/CXX_FLAGS_INIT)
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} \
    -DCRYSTALLESS \
    -DARM_MATH_CM0PLUS \
    -DF_CPU=48000000L \
    -DUSE_NOUSB \
    -DSAM21D \
")

set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} \
    -DCRYSTALLESS \
    -DARM_MATH_CM0PLUS \
    -DF_CPU=48000000L \
    -DUSE_NOUSB \
    -DSAM21D \
")

# Include variant headers (must be BEFORE CMSIS for sam.h to work)
include_directories(BEFORE "${CURIOSITY_VARIANT_DIR}")

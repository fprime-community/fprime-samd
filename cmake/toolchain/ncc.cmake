####
# microchip_curiosity.cmake:
#
# Toolchain file for SAMD21 Curiosity Nano (SAMD21G17D)
# Hardware: SAMD21G17D (128KB flash, 16KB RAM)
# USB stack disabled - USB is connected to debugger only, not MCU
####

# This is required for <sam.h> to include the correct device header
add_compile_definitions(
    __SAMD21J17A__
    __SAMD21J17D__
)

# Enable LTO for this board (has limited flash)
set(SAMD21_LTO ON)
set(SAMD21_MTB OFF)

# Set the board type
set(BOARD_TYPE "NCC")

# Variant directory (relative to this toolchain file)
get_filename_component(VARIANT_DIR "${CMAKE_CURRENT_LIST_DIR}/samd21/ncc" ABSOLUTE)

# Set linker script path (will be used after common toolchain is included)
set(SAMD21_LINKER_SCRIPT "${VARIANT_DIR}/linker_scripts/flash_without_bootloader.ld")

# Include common samd21 toolchain setup
include("${CMAKE_CURRENT_LIST_DIR}/samd21-common.cmake")

# Add linker script AFTER common toolchain (so it doesn't get overwritten)
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT} \
    -T${SAMD21_LINKER_SCRIPT} \
    -L${VARIANT_DIR}/linker_scripts \
")

# Board-specific defines (after toolchain, so they append to CMAKE_C/CXX_FLAGS_INIT)
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} \
    -DARM_MATH_CM0PLUS \
    -DF_CPU=48000000L \
    -DUSE_NOUSB \
    -DSAM21D \
")

set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} \
    -DARM_MATH_CM0PLUS \
    -DF_CPU=48000000L \
    -DUSE_NOUSB \
    -DSAM21D \
")

# Include variant headers (must be BEFORE CMSIS for sam.h to work)
include_directories(BEFORE "${VARIANT_DIR}")

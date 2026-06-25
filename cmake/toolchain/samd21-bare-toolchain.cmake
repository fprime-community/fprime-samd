####
# samd21-bare-toolchain.cmake:
#
# Pure bare-metal SAMD21 toolchain - no Arduino-CLI invocation at build time.
# Uses arm-none-eabi-gcc toolchain installed via Arduino-CLI, but hardcodes
# all paths, flags, and includes.
#
# Requirements:
#   - arm-none-eabi-gcc must be installed (via arduino-cli core install adafruit:samd)
#   - CMSIS headers must be available (installed with adafruit:samd core)
#
# @author tumbar
####

cmake_minimum_required(VERSION 3.19)

# Toolchain paths - using Arduino-installed arm-none-eabi-gcc
set(TOOLCHAIN_PREFIX "arm-none-eabi-")
set(TOOLCHAIN_ROOT "$ENV{HOME}/.arduino15/packages/adafruit/tools/arm-none-eabi-gcc/9-2019q4")
set(TOOLCHAIN_BIN "${TOOLCHAIN_ROOT}/bin")

# Compilers and tools
set(CMAKE_C_COMPILER "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}g++")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}gcc")
set(CMAKE_AR "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}ar")
set(CMAKE_LINKER "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}g++")
set(CMAKE_OBJCOPY "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}objcopy")
set(CMAKE_OBJDUMP "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}objdump")
set(CMAKE_NM "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}nm")
set(CMAKE_SIZE "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}size")
set(CMAKE_RANLIB "${TOOLCHAIN_BIN}/${TOOLCHAIN_PREFIX}ranlib")

# CMake needs this for try_compile to work
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# CMSIS and device headers
set(CMSIS_ROOT "$ENV{HOME}/.arduino15/packages/adafruit/tools")
set(CMSIS_CORE_INCLUDE "${CMSIS_ROOT}/CMSIS/5.4.0/CMSIS/Core/Include")
set(CMSIS_DEVICE_INCLUDE "${CMSIS_ROOT}/CMSIS-Atmel/1.2.2/CMSIS/Device/ATMEL")
set(CMSIS_DSP_INCLUDE "${CMSIS_ROOT}/CMSIS/5.4.0/CMSIS/DSP/Include")

# Common CPU flags
set(CPU_FLAGS "-mcpu=cortex-m0plus -mthumb")

# Common C/C++ flags
set(COMMON_FLAGS "\
    ${CPU_FLAGS} \
    -Os \
    -g \
    -ffunction-sections \
    -fdata-sections \
    -nostdlib \
    --param max-inline-insns-single=500 \
    -Werror=return-type \
")

# C-specific flags
set(CMAKE_C_FLAGS_INIT "\
    ${COMMON_FLAGS} \
    -std=gnu11 \
")

# C++-specific flags
set(CMAKE_CXX_FLAGS_INIT "\
    ${COMMON_FLAGS} \
    -std=gnu++11 \
    -fno-threadsafe-statics \
    -fno-rtti \
    -fno-exceptions \
")

# ASM flags
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")

# Linker flags (must be set via CMAKE_EXE_LINKER_FLAGS_INIT for cross-compilation)
set(CMAKE_EXE_LINKER_FLAGS_INIT "\
    ${CPU_FLAGS} \
    -Os \
    -Wl,--gc-sections \
    -Wl,--cref \
    -Wl,--check-sections \
    -Wl,--unresolved-symbols=report-all \
    -Wl,--warn-common \
    -Wl,--warn-section-align \
    -Wl,-Map,link.map \
    --specs=nano.specs \
    --specs=nosys.specs \
    -L${CMSIS_ROOT}/CMSIS/5.4.0/CMSIS/Lib/GCC/ \
")

# AR flags
set(CMAKE_C_ARCHIVE_CREATE "<CMAKE_AR> rcs <TARGET> <OBJECTS>")
set(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> rcs <TARGET> <OBJECTS>")

# Linker command
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

# Include directories
include_directories(SYSTEM
    "${CMSIS_CORE_INCLUDE}"
    "${CMSIS_DSP_INCLUDE}"
    "${CMSIS_DEVICE_INCLUDE}"
)

# Executable suffix
if (NOT DEFINED FPRIME_ARDUINO_EXECUTABLE_SUFFIX)
    set(FPRIME_ARDUINO_EXECUTABLE_SUFFIX ".elf")
endif()
set(CMAKE_EXECUTABLE_SUFFIX "${FPRIME_ARDUINO_EXECUTABLE_SUFFIX}" CACHE INTERNAL "" FORCE)

####
# Function `finalize_samd21_executable`:
#
# Bare-metal finalization - sets up post-build steps (hex files, size calculation, etc).
####
function(finalize_samd21_executable)
    include(API)
    if (DEFINED FPRIME_SUBBOUILD_TARGETS)
        return()
    endif()

    set_target_properties("${FPRIME_CURRENT_MODULE}" PROPERTIES SUFFIX ".elf")

    # Post-build commands
    set(COMMAND_SET_ARGUMENTS)

    # Generate binary file
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE:${FPRIME_CURRENT_MODULE}>"
            "$<TARGET_FILE:${FPRIME_CURRENT_MODULE}>.bin"
    )

    # Generate hex file
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_OBJCOPY}" -O ihex -R .eeprom
            "$<TARGET_FILE:${FPRIME_CURRENT_MODULE}>"
            "$<TARGET_FILE:${FPRIME_CURRENT_MODULE}>.hex"
    )

    # Print size
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_SIZE}" -A "$<TARGET_FILE:${FPRIME_CURRENT_MODULE}>"
    )

    # Copy artifacts to install directory
    set(ARTIFACTS_DIR "${FPRIME_INSTALL_DEST}/${TOOLCHAIN_NAME}/${FPRIME_CURRENT_MODULE}/bin")
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ARTIFACTS_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${FPRIME_CURRENT_MODULE}>*"
            "${ARTIFACTS_DIR}"
    )

    # Generate objdump output
    set(OBJDUMP_SCRIPT "${CMAKE_BINARY_DIR}/objdump_${FPRIME_CURRENT_MODULE}.sh")
    file(GENERATE OUTPUT "${OBJDUMP_SCRIPT}"
        CONTENT "#!/bin/sh\n${CMAKE_OBJDUMP} -xd $<TARGET_FILE:${FPRIME_CURRENT_MODULE}> > $<TARGET_FILE_DIR:${FPRIME_CURRENT_MODULE}>/$<TARGET_FILE_BASE_NAME:${FPRIME_CURRENT_MODULE}>.objdump\n")
    list(APPEND COMMAND_SET_ARGUMENTS COMMAND sh "${OBJDUMP_SCRIPT}")
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE_DIR:${FPRIME_CURRENT_MODULE}>/$<TARGET_FILE_BASE_NAME:${FPRIME_CURRENT_MODULE}>.objdump"
            "${ARTIFACTS_DIR}/"
    )

    # Generate nm output with function sizes
    set(NM_SCRIPT "${CMAKE_BINARY_DIR}/nm_${FPRIME_CURRENT_MODULE}.sh")
    file(GENERATE OUTPUT "${NM_SCRIPT}"
        CONTENT "#!/bin/sh\n${CMAKE_NM} --print-size --size-sort $<TARGET_FILE:${FPRIME_CURRENT_MODULE}> > $<TARGET_FILE_DIR:${FPRIME_CURRENT_MODULE}>/$<TARGET_FILE_BASE_NAME:${FPRIME_CURRENT_MODULE}>.nm\n")
    list(APPEND COMMAND_SET_ARGUMENTS COMMAND sh "${NM_SCRIPT}")
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE_DIR:${FPRIME_CURRENT_MODULE}>/$<TARGET_FILE_BASE_NAME:${FPRIME_CURRENT_MODULE}>.nm"
            "${ARTIFACTS_DIR}/"
    )

    # Copy linker map file
    list(APPEND COMMAND_SET_ARGUMENTS
        COMMAND "${CMAKE_COMMAND}" -E copy
            "${CMAKE_BINARY_DIR}/link.map"
            "${ARTIFACTS_DIR}/$<TARGET_FILE_BASE_NAME:${FPRIME_CURRENT_MODULE}>.map"
    )

    add_custom_command(
        TARGET "${FPRIME_CURRENT_MODULE}" POST_BUILD ${COMMAND_SET_ARGUMENTS}
    )
endfunction(finalize_samd21_executable)

if (CMAKE_DEBUG_OUTPUT)
    message(STATUS "[samd21-bare-toolchain] Detected C    compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "[samd21-bare-toolchain] Detected C++  compiler: ${CMAKE_CXX_COMPILER}")
    message(STATUS "[samd21-bare-toolchain] Detected linker: ${CMAKE_LINKER}")
    message(STATUS "[samd21-bare-toolchain] CMSIS Core: ${CMSIS_CORE_INCLUDE}")
    message(STATUS "[samd21-bare-toolchain] CMSIS Device: ${CMSIS_DEVICE_INCLUDE}")
endif()

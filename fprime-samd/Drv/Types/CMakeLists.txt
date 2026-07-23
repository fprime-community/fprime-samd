####
# F Prime CMakeLists.txt:
#
# SOURCES: list of source files (to be compiled)
# AUTOCODER_INPUTS: list of files to be passed to the autocoders
# DEPENDS: list of libraries that this module depends on
#
# More information in the F´ CMake API documentation:
# https://fprime.jpl.nasa.gov/latest/docs/reference/api/cmake/API/
#
####

# Select hardware implementation based on platform
if (FPRIME_PLATFORM STREQUAL "Samd21")
    # SAMD21 target build - use real hardware implementation
    set(TYPES_HARDWARE_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/PinMux.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/CriticalSection.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SercomIsr.cpp"
    )

    if (SAMD21_LTO)
        set_source_files_properties(
            "${CMAKE_CURRENT_LIST_DIR}/SercomIsr.cpp"
            PROPERTIES COMPILE_FLAGS "-fno-lto"
        )
    endif()
else()
    # Linux/test build - use stub implementations
    set(TYPES_HARDWARE_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/PinMuxStub.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/CriticalSectionStub.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/SercomIsrStub.cpp"
    )
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Dma.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/Types.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ThinBuffer.cpp"
        ${TYPES_HARDWARE_SOURCE}
)

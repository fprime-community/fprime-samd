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
    set(ADC_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/AdcDriverHardware.cpp")
    set(ADC_ISR_SOURCE "${CMAKE_CURRENT_LIST_DIR}/AdcDriverIsr.cpp")
else()
    # Linux/test build - use stub implementation
    set(ADC_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/AdcDriverHardwareStub.cpp")
    set(ADC_ISR_SOURCE "")  # No ISR for test builds
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriver.cpp"
        ${ADC_HARDWARE_SOURCE}
        ${ADC_ISR_SOURCE}
   DEPENDS
       config-fprime-samd
)

# Disable LTO on ISR file to avoid ADC_Handler ISR getting optimized away
if (SAMD21_LTO AND ADC_ISR_SOURCE)
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriverIsr.cpp"
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )
endif()

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/AdcDriverTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/AdcDriverTester.cpp"
        # Use stub hardware for unit tests
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriverHardwareStub.cpp"
    DEPENDS
        STest # For rules-based testing
    UT_AUTO_HELPERS
)

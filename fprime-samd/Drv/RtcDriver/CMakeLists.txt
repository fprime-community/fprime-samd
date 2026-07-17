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

# Module names are derived from the path from the nearest project/library/framework
# root when not specifically overridden by the developer, i.e. the module defined by
# `MyProj/Some/Path/CMakeLists.txt` will be named `MyProj_Some_Path`.

# Select hardware implementation based on platform
if (FPRIME_PLATFORM STREQUAL "Samd21")
    # SAMD21 target build - use real hardware implementation
    set(RTC_HARDWARE_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriverHardware.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriverIsr.cpp"
    )
else()
    # Linux/test build - use stub implementation
    set(RTC_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/RtcDriverHardwareStub.cpp")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriver.cpp"
        ${RTC_HARDWARE_SOURCE}
#   DEPENDS
#       MyPackage_MyOtherModule
)

# Disable LTO for ISR file to ensure RTC_Handler ISR is not optimized away
if (SAMD21_LTO)
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriverIsr.cpp"
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )
endif()

register_os_implementation("RawTime" Samd21)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RtcDriverTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/RtcDriverTester.cpp"
        # Use stub hardware for unit tests
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriverHardwareStub.cpp"
        # Include RawTime for direct testing
        "${CMAKE_CURRENT_LIST_DIR}/RawTime.cpp"
    DEPENDS
        STest
    UT_AUTO_HELPERS
)

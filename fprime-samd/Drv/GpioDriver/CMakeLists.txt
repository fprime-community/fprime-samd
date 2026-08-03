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
    set(GPIO_HARDWARE_SOURCE
        "${CMAKE_CURRENT_LIST_DIR}/GpioDriverHardware.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/GpioDriverIsr.cpp"
    )
else()
    # Linux/test build - use stub implementation
    set(GPIO_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/GpioDriverHardwareStub.cpp")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/GpioDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/GpioDriver.cpp"
        ${GPIO_HARDWARE_SOURCE}
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/GpioDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/GpioDriverTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/GpioDriverTester.cpp"
        # Use stub hardware for unit tests
        "${CMAKE_CURRENT_LIST_DIR}/GpioDriverHardwareStub.cpp"
    DEPENDS
        STest # For rules-based testing
    UT_AUTO_HELPERS
)

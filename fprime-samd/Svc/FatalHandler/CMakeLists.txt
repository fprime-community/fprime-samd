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
    set(FATAL_HANDLER_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/FatalHandlerHardware.cpp")
else()
    # Linux/test build - use stub implementation
    set(FATAL_HANDLER_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/test/ut/FatalHandlerHardwareStub.cpp")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FatalHandler.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/FatalHandler.cpp"
        ${FATAL_HANDLER_HARDWARE_SOURCE}
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/FatalHandler.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FatalHandlerHardware.hpp"
    DEPENDS
        config-fprime-samd
)

# Fw_AssertHook implementation that reports failed assertions through the
# FatalHandler instance. Chosen by the samd21 platform.
register_fprime_implementation(
    Samd21_AssertHook_FatalHandler
  IMPLEMENTS
    Fw_AssertHook
  DEPENDS
    Fw_Types
    ${FPRIME_CURRENT_MODULE}
  SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/FatalHandlerAssertHook.cpp"
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/FatalHandler.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FatalHandlerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FatalHandlerTester.cpp"
        # Use stub hardware for unit tests
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/FatalHandlerHardwareStub.cpp"
    DEPENDS
        STest # For rules-based testing
    CHOOSES_IMPLEMENTATIONS
        # Exercise the same assert path as flight
        Samd21_AssertHook_FatalHandler
    UT_AUTO_HELPERS
)

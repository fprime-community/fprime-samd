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
    set(USART_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/UsartDriverHardware.cpp")
else()
    # Linux/test build - use stub implementation
    set(USART_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/UsartDriverHardwareStub.cpp")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/UsartDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/UsartDriver.cpp"
        ${USART_HARDWARE_SOURCE}
  DEPENDS
      Drv_Interfaces
      fprime-samd_Drv_Types
)

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/UsartDrv.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/UsartDrvTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/UsartDrvTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

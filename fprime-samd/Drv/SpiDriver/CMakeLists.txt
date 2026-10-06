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

if (FPRIME_PLATFORM STREQUAL "Samd21")
    # SAMD21 target build - use real hardware implementation
    set(SPI_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/SpiDriverHardware.cpp")
else()
    # Linux/test build - use stub implementation
    set(SPI_HARDWARE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/SpiDriverHardwareStub.cpp")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/SpiDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/SpiDriver.cpp"
        ${SPI_HARDWARE_SOURCE}
  DEPENDS
      Drv_Interfaces
      fprime-samd_Drv_Types
)

register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/SpiDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/SpiDriverTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/SpiDriverTester.cpp"
        # Use stub hardware for unit tests
        "${CMAKE_CURRENT_LIST_DIR}/SpiDriverHardwareStub.cpp"
    DEPENDS
        STest
    UT_AUTO_HELPERS
)

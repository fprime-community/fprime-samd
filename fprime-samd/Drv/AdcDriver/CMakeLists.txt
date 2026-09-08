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

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriver.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriver.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriverIsr.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/TempSensorCalibration.cpp"
   DEPENDS
       config-fprime-samd
)

# Disable LTO on ISR file to avoid DMAC_Handler ISR getting optimized away
if (SAMD21_LTO)
    set_source_files_properties(
        "${CMAKE_CURRENT_LIST_DIR}/AdcDriverIsr.cpp"
        PROPERTIES COMPILE_FLAGS "-fno-lto"
    )
endif()

### Unit Tests ###
# register_fprime_ut(
#     AUTOCODER_INPUTS
#         "${CMAKE_CURRENT_LIST_DIR}/AdcDriver.fpp"
#     SOURCES
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/AdcDriverTestMain.cpp"
#         "${CMAKE_CURRENT_LIST_DIR}/test/ut/AdcDriverTester.cpp"
#     DEPENDS
#         STest # For rules-based testing
#     UT_AUTO_HELPERS
# )

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

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/SeqConfig/")

# Unit tests exercise names (STARTUP/SENSORS/SENSORS_RECOVER) that mirror a real
# deployment's names rather than the GENERIC default, so override for BUILD_TESTING only.
if(BUILD_TESTING)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test/ut-config/")
endif()

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Seq.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/Seq.cpp"
    DEPENDS
        fprime-samd_Svc_Types
        fprime-samd-seq-config
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Seq.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/SeqTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/SeqTester.cpp"
    DEPENDS
        STest # For rules-based testing
        Fw_Test
        fprime-samd_Svc_Types
    UT_AUTO_HELPERS
)

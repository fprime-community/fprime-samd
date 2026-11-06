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
        "${CMAKE_CURRENT_LIST_DIR}/PrmStorage.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/PrmStorage.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/PrmStorage.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/PrmStorageLayout.hpp"
    DEPENDS
        Drv_ByteStreamDriverModel
        Svc_Sched
        fprime-devices_Fram_FramConfig
        fprime-devices_Fram_Ports
        fprime-devices_Fram_Types
        config-fprime-samd
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PrmStorage.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/PrmStorageTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/PrmStorageTester.cpp"
    DEPENDS
        STest # For rules-based testing
        Fw_Test # For REQUIREMENT() traceability
    UT_AUTO_HELPERS
)

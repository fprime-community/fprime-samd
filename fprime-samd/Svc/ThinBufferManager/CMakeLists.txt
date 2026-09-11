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
        "${CMAKE_CURRENT_LIST_DIR}/ThinBufferManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/ThinBufferManagerComponentImpl.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/ThinBufferManager.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ThinBufferManagerComponentImpl.hpp"
    DEPENDS
        fprime-samd_Drv_Types
        config-fprime-samd
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/ThinBufferManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ThinBufferManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/ThinBufferManagerTester.cpp"
    DEPENDS
        STest
        fprime-samd_Drv_Types
        config-fprime-samd
    UT_AUTO_HELPERS
)

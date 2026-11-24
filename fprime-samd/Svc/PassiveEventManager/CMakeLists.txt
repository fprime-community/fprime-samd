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

register_fprime_library(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PassiveEventManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/PassiveEventManager.cpp"
#   DEPENDS
#       MyPackage_MyOtherModule
)

### Unit Tests ###
register_fprime_ut(
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/PassiveEventManager.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/PassiveEventManagerTestMain.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/test/ut/PassiveEventManagerTester.cpp"
    DEPENDS
        STest
        Fw_Test # For REQUIREMENT() traceability
    UT_AUTO_HELPERS
)

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
        "${CMAKE_CURRENT_LIST_DIR}/StaticCmdDispatcher.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/StaticCmdDispatcher.cpp"
)

### Unit Tests ###
# Run against test/StaticTopology, whose dispatch table static-cmd-dispatcher generates (cmake/static_topology.cmake)
if (FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED)
    register_fprime_ut(
        SOURCES
            "${CMAKE_CURRENT_LIST_DIR}/test/ut/StaticCmdDispatcherTestMain.cpp"
            "${CMAKE_CURRENT_LIST_DIR}/test/ut/StaticCmdDispatcherTests.cpp"
        DEPENDS
            Fw_Test # For REQUIREMENT() traceability
    )
    fprime_samd_link_static_topology()
endif()

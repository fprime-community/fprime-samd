####
# static_topology.cmake:
#
# Unit tests of the Static* components run against test/StaticTopology, a native-only
# deployment topology built with the static-* autocoders. Those autocoders only run on normal
# modules, never on unit-test targets, and need a deployment topology as input; so the
# topology is one module and each component's unit test links it.
#
# The topology compiles against the project's configuration, so it is opt-in:
# -DFPRIME_SAMD_STATIC_TOPOLOGY_TESTS=ON in a native unit-test build.
####
include_guard()

option(FPRIME_SAMD_STATIC_TOPOLOGY_TESTS "Build fprime-samd's StaticTopology and the unit tests run against it" OFF)

if (FPRIME_SAMD_STATIC_TOPOLOGY_TESTS AND BUILD_TESTING AND FPRIME_TOOLCHAIN_NAME STREQUAL "native")
    set(FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED ON)
else()
    set(FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED OFF)
endif()

####
# Function `fprime_samd_link_static_topology`:
#
# Links the StaticTopology into the current component's unit test. Call right after the
# component's own register_fprime_ut(), guarded by FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED:
#
#     if (FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED)
#         register_fprime_ut(SOURCES ... DEPENDS ...)
#         fprime_samd_link_static_topology()
#     endif()
#
# register_fprime_ut() stays literal in the component's CMakeLists.txt: coverage tooling
# (nasa/fprime-actions discover.py) detects a module's unit tests by that text.
#
# The Static* components call functions generated into the topology library (e.g.
# StaticCmdDispatcher::lookupDispatchPort), and the topology library constructs the
# components, so the libraries depend on each other. The topology is linked whole-archive:
# every generated function is in the executable however late the linker reaches a
# component's library.
####
function(fprime_samd_link_static_topology)
    set(UT_TARGET "${FPRIME_CURRENT_MODULE}_${FPRIME__INTERNAL_UT_TARGET}")
    if (NOT TARGET "${UT_TARGET}")
        return()
    endif()
    target_link_libraries("${UT_TARGET}" PRIVATE
        "$<LINK_LIBRARY:WHOLE_ARCHIVE,fprime-samd_StaticTopology_Top>")
endfunction()

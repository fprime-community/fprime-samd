####
# The StaticTopology deployment topology. A module (not a deployment): there is no
# executable, the Static* components' unit tests link it (see library.cmake,
# fprime_samd_add_static_topology_ut). Building it runs the static-* autocoders.
####
register_fprime_module(
        fprime-samd_StaticTopology_Top
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/instances.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/topology.fpp"
    SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/StaticTopologyFixture.cpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/TopTopologyDefs.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/StaticTopologyFixture.hpp"
    DEPENDS
        fprime-samd_StaticTopology_Target
        fprime-samd_StaticTopology_Ground
        fprime-samd_StaticTopology_EventLog
        fprime-samd_StaticTopology_Clock
        Os
        gtest
)

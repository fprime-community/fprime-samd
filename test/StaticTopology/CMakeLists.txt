####
# StaticTopology: native-only deployment topology the Static* components' unit tests run against.
#
# The static-* build autocoders only run on normal modules, never on unit-test targets, and
# they need a deployment topology as input. So the topology is built once here, as a module,
# and each component's unit test links it (fprime_samd_link_static_topology, cmake/static_topology.cmake)
# and drives the real instances through test-harness components. Edge cases of the generated
# code are covered by adding instances, channels or packets here.
#
# Built only with -DFPRIME_SAMD_STATIC_TOPOLOGY_TESTS=ON in a unit-test build: it compiles
# against the project's configuration, so it is opt-in for consumers of this library.
####
restrict_platforms(native)

add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Target/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Ground/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/EventLog/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Clock/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Top/")

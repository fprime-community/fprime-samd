# FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED and fprime_samd_link_static_topology, used by the
# Static* components' CMakeLists.txt
include("${CMAKE_CURRENT_LIST_DIR}/cmake/static_topology.cmake")

# Add helper library for MCU bootup
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Mcu/")

# Add fprime-samd subdirectories
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/default/samd-config")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Drv/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Overrides")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Os")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Svc/")

# Unit tests of the Static* components run against this topology (cmake/static_topology.cmake)
if (FPRIME_SAMD_STATIC_TOPOLOGY_ENABLED)
    add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test/StaticTopology")
endif()

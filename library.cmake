# Add fprime-samd subdirectories
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/config")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Drv/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Overrides")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Os")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Svc/")

# Build the MCU support library (startup, reset, handlers)
# This replaces Arduino core's main.cpp, cortex_handlers.c, and startup.c

# FindCMSIS needs to be in module path before we call find_package
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE INTERNAL "")
find_package(CMSIS REQUIRED)

add_library(fprime_samd_mcu STATIC
    "${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Mcu/Reset.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Mcu/Handlers.c"
    "${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Mcu/SystemInit.c"
)

target_include_directories(fprime_samd_mcu PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/fprime-samd"
)

target_link_libraries(fprime_samd_mcu PUBLIC
    CMSIS::Headers
)

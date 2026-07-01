
# Build the MCU support library (startup, reset, handlers)
# This replaces Arduino core's main.cpp, cortex_handlers.c, and startup.c

# FindCMSIS needs to be in module path before we call find_package
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_LIST_DIR}/../../cmake" CACHE INTERNAL "")
find_package(CMSIS REQUIRED)

add_library(fprime_samd_mcu STATIC
        "${CMAKE_CURRENT_LIST_DIR}/reset.c"
        "${CMAKE_CURRENT_LIST_DIR}/SystemInit.c"
)

target_link_libraries(fprime_samd_mcu PUBLIC
        CMSIS::Headers
)

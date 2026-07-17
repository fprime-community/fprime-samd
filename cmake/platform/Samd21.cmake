####
# Samd21.cmake:
#
# Platform configuration for SAMD21 bare-metal targets.
####

# Set executable suffix (.elf for bare-metal)
set(CMAKE_EXECUTABLE_SUFFIX ".elf" CACHE INTERNAL "" FORCE)

# Platform types directory
set(SAMD21_TYPES_DIR "${CMAKE_CURRENT_LIST_DIR}/samd21/Platform")
message(STATUS "[fprime-samd21] Including Types Directory: ${SAMD21_TYPES_DIR}")
include_directories("${CMAKE_CURRENT_LIST_DIR}")
add_fprime_subdirectory("${SAMD21_TYPES_DIR}")

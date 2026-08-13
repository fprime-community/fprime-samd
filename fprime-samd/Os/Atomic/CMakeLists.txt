####
# Atomic Operations Library for Cortex-M0+
#
# Provides GCC atomic builtin implementations for Cortex-M0+ which lacks
# hardware atomic instruction support.
####

add_library(samd21_atomic_stubs STATIC
    "${CMAKE_CURRENT_LIST_DIR}/atomic_stubs.c"
)

message(STATUS "[Samd21] Built atomic operation stubs for Cortex-M0+")

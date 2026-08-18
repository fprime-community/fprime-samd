restrict_platforms(Samd21)
add_custom_target("${FPRIME_CURRENT_MODULE}")

# Add atomic operation stubs for Cortex-M0+
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/Atomic")

register_os_implementation("Console" Samd21)
register_os_implementation("Mutex;ConditionVariable" Samd21)

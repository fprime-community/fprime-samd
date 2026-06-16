restrict_platforms(ArduinoFw)
add_custom_target("${FPRIME_CURRENT_MODULE}")

register_os_implementation("Console" Samd21)
register_os_implementation("Mutex;ConditionVariable" Samd21)

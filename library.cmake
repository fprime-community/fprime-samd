# Add helper library for MCU bootup
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Mcu/")

# Add fprime-samd subdirectories
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/config-samd")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Drv/")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Overrides")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Os")
add_fprime_subdirectory("${CMAKE_CURRENT_LIST_DIR}/fprime-samd/Svc/")

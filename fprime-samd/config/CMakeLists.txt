register_fprime_config(
    fprime-samd-config
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriverConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/DmaDriverConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/UsartDriverConfig.hpp"
    INTERFACE
)

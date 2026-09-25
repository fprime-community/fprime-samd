if(FPRIME_PLATFORM STREQUAL "Samd21")
    set(SAMD21_ONLY_CFG_OVERRIDES
        CONFIGURATION_OVERRIDES "${CMAKE_CURRENT_LIST_DIR}/OsDelegateMutex.hpp"
    )
else()
    set(SAMD21_ONLY_CFG_OVERRIDES "")
endif()

register_fprime_config(
        config-fprime-samd
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/Samd21AdcConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/Samd21SpiConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/Samd21I2cConfig.fpp"
        "${CMAKE_CURRENT_LIST_DIR}/Samd21StaticTlmPacketizerConfig.fpp"
    HEADERS
        "${CMAKE_CURRENT_LIST_DIR}/RtcDriverConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/DmaDriverConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/UsartDriverConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/I2cDriverConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/FramerConfig.hpp"
        "${CMAKE_CURRENT_LIST_DIR}/ThinBufferManagerConfig.hpp"
    ${SAMD21_ONLY_CFG_OVERRIDES}
    DEPENDS
        Fw_Types
    INTERFACE
)

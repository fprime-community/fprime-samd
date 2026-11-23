####
# StaticTopology configuration: overrides of lib/fprime-samd/default/samd-config that the
# StaticTopology needs. Registered only when the StaticTopology is built (opt-in, native
# unit-test builds), so flight builds keep the library defaults.
####
register_fprime_config(
        fprime-samd_StaticTopology_config
    CONFIGURATION_OVERRIDES
        "${CMAKE_CURRENT_LIST_DIR}/Samd21StaticTlmPacketizerConfig.fpp"
    INTERFACE
)

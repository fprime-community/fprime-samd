register_fprime_config(
        fprime-samd-seq-config
    AUTOCODER_INPUTS
        "${CMAKE_CURRENT_LIST_DIR}/SeqNames.fpp"
    DEPENDS
        "${FPRIME_GLOBAL_INTERFACE_TARGET}"
)

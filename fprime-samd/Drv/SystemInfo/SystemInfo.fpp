module Samd21 {
    @ A component for reporting system information about the SAMD21 MCU
    passive component SystemInfo {

        @ System information is reported one field per channel. A single struct-valued
        @ channel would serialize to 28 bytes and not fit FW_TLM_BUFFER_MAX_SIZE; each
        @ channel below fits an 8 byte buffer.

        @ Cause of the most recent reset
        telemetry ResetCause: Samd21.ResetReason

        @ Leading 64 bits of the project HEAD commit this image was built from
        telemetry ProjectCommit: U64 format "{x}"

        @ Leading 64 bits of the fprime HEAD commit this image was built from
        telemetry FprimeCommit: U64 format "{x}"

        @ Leading 64 bits of the fprime-samd HEAD commit this image was built from
        telemetry SamdCommit: U64 format "{x}"

        @ System information gathered at startup
        event SystemInfo(
            resetReason: Samd21.ResetReason
            projectCommit: U64
            fprimeCommit: U64
            samdCommit: U64
        ) \
            severity activity high \
            id 0 \
            format "SAMD MCU is starting up after {} reset, built from project={x} fprime={x} samd={x}"

        @ Port for requesting the current time
        time get port timeCaller

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry channels handling
        import Fw.Channel

    }
}

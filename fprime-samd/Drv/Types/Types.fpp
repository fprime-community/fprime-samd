module Samd21 {

    @ The set of all SERCOM instances on the SAMD21
    enum SercomKind : U8 {
        SERCOM_0
        SERCOM_1
        SERCOM_2
        SERCOM_3
        SERCOM_4
        SERCOM_5
    }

    enum ResetReason : U8 {
        @ Set when a power-on reset occurs.
        POWER_ON

        @ Set when a 1.2V core voltage drop triggers a reset.
        BROWN_OUT_12

        @ Set when the 3.3V/analog voltage drops below threshold
        BROWN_OUT_33

        @ Set when the external RESET is pulled low
        EXTERNAL

        @ Set when the watchdog timer expires
        WATCHDOG_TIMER

        @ Set when the CPU core requests a system reset (FW_ASSERT)
        SYSTEM

        @ None of the RCAUSE bits are set... (this shouldn't happen)
        UNKNOWN
    }
}

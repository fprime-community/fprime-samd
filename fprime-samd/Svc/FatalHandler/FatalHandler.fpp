module Samd21 {
    @ Fatal handler for SAMD21 platform
    @
    @ Failed assertions reach the most recently constructed instance through the
    @ link-time Fw_AssertHook implementation Samd21_AssertHook_FatalHandler. It
    @ sends an AssertionFailed event packet synchronously out of fatalOut,
    @ bypassing the normal event path, then resets the MCU.
    passive component FatalHandler {

        import Fw.Event
        time get port timeGet

        @ FATAL event receive port
        sync input port FatalReceive: Svc.FatalEvent

        @ Synchronous output for the assertion packet (connect to a SyncFramer)
        output port fatalOut: Fw.Com

        event AssertionFailed(
            file: U32,
            lineNo: U32,
            numArgs: FwSizeStoreType,
            arg1: I32,
            arg2: I32,
            arg3: I32,
            arg4: I32,
            arg5: I32,
            arg6: I32,
        ) severity fatal id 0x0 format "Assertion failed: 0x{x}:{} n = {} ({}, {}, {}, {}, {}, {})"

    }
}

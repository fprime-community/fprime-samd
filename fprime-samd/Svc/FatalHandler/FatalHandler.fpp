module Samd21 {
    @ Fatal handler for SAMD21 platform
    passive component FatalHandler {

        import Fw.Event
        time get port timeGet

        @ FATAL event receive port
        sync input port FatalReceive: Svc.FatalEvent

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
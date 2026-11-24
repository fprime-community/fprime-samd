// ======================================================================
// \title  reset_assert_hooks.cpp
// \author Andrei Tumbar
// \brief  cpp file for assert hook functions using reset implementation
//
// For deployments without a Samd21::FatalHandler (which choose this over the
// platform's Samd21_AssertHook_FatalHandler). Nothing is reported; the MCU is reset.
// ======================================================================
#include <sam.h>
#include <Fw/Types/assert_hook.hpp>

void Fw::terminateOnAssert() {
    NVIC_SystemReset();

    while (true) {
    }
}

void Fw::printAssertMessage(const CHAR* msg) {}

void Fw::formatAssertMessage(FILE_NAME_ARG file,
                             FwSizeType lineNo,
                             FwSizeType numArgs,
                             FwAssertArgType arg1,
                             FwAssertArgType arg2,
                             FwAssertArgType arg3,
                             FwAssertArgType arg4,
                             FwAssertArgType arg5,
                             FwAssertArgType arg6,
                             CHAR* destBuffer,
                             FwSizeType buffSize) {
    if ((destBuffer != nullptr) && (buffSize > 0)) {
        destBuffer[0] = '\0';
    }
}

// ======================================================================
// \title  FatalHandlerAssertHook.cpp
// \author tumbar
// \brief  Fw_AssertHook implementation reporting through Samd21::FatalHandler
//
// Link-time selected (Samd21_AssertHook_FatalHandler). Failed assertions are
// sent out of the FatalHandler instance's fatalOut port and the MCU is reset.
// Before a FatalHandler is constructed nothing is reported; the MCU is reset.
// ======================================================================
#include <Fw/Types/assert_hook.hpp>
#include "fprime-samd/Svc/FatalHandler/FatalHandler.hpp"

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
    Samd21::FatalHandler::formatAssert(file, lineNo, numArgs, arg1, arg2, arg3, arg4, arg5, arg6, destBuffer, buffSize);
}

void Fw::printAssertMessage(const CHAR* msg) {
    Samd21::FatalHandler::sendAssert(msg);
}

void Fw::terminateOnAssert() {
    Samd21::FatalHandler::terminate();
}

// ======================================================================
// \title  FatalHandlerHardware.cpp
// \author tumbar
// \brief  Hardware implementation for SAMD21 fatal handling
//
// This file is compiled for SAMD21 target builds only. For Linux/test builds,
// FatalHandlerHardwareStub.cpp is used instead.
// ======================================================================

#include "fprime-samd/Svc/FatalHandler/FatalHandlerHardware.hpp"
#include "fprime-samd/Mcu/Delay.hpp"

namespace Samd21 {
namespace FatalHandlerHardware {

void FatalHandlerHal::disableInterrupts() {
    __disable_irq();
}

void FatalHandlerHal::delayMs(U32 ms) {
    delay(ms);
}

void FatalHandlerHal::reset() {
    NVIC_SystemReset();
}

}  // namespace FatalHandlerHardware
}  // namespace Samd21

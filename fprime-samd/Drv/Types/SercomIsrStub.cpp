// ======================================================================
// \title  SercomIsrStub.cpp
// \author tumbar
// \brief  Stub PinMux implementation for Linux/test builds
// ======================================================================

#include <Fw/FPrimeBasicTypes.hpp>
#include "fprime-samd/Drv/Types/SercomIsr.hpp"

namespace Samd21 {
void SercomIsr::registerHandler(SercomKind sercom, void (*callback)(SercomKind, void*), void* data) {}
}  // namespace Samd21

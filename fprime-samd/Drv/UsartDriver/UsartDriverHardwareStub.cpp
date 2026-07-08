// ======================================================================
// \title  UsartDriverHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for USART peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, UsartDriverHardware.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/UsartDriver/UsartDriverHardware.hpp"

namespace Samd21 {
namespace UsartHardware {

void UsartHal::configure(SercomKind sercom,
                         UsartDriver::RxPinOut rx,
                         UsartDriver::TxPinOut tx,
                         UsartDriver::ClockMode clock,
                         UsartDriver::CommunicationMode mode,
                         UsartDriver::BaudRate baud_rate,
                         UsartDriver::DataOrder data_order,
                         UsartDriver::DataBits data_bits,
                         UsartDriver::StopBits stop_bits,
                         UsartDriver::Parity parity) {
    // Stub: no-op for tests
}

void UsartHal::sendSync(SercomKind sercom, const U8* data, U32 size) {
    // Stub: no-op for tests
}

U32 UsartHal::getDataRegisterAddress(SercomKind sercom) {
    // Stub: return a dummy address for tests
    return 0;
}

}  // namespace UsartHardware
}  // namespace Samd21

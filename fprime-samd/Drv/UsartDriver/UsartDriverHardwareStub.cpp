// ======================================================================
// \title  UsartDriverHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for USART peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, UsartDriverHardware.cpp is used instead.
//
// The stub records the arguments of the last hardware call so unit tests can
// verify what the driver asked the peripheral to do without any real MCU.
// ======================================================================

#include <cstring>
#include "fprime-samd/Drv/UsartDriver/UsartDriverHardware.hpp"

namespace Samd21 {
namespace UsartHardware {

static StubState s_state = {};

StubState& getStubState() {
    return s_state;
}

void resetStubState() {
    // Assign a fresh value-initialized instance rather than memset: several
    // members (e.g. SercomKind) are autocoded classes with vtables that
    // memset would corrupt.
    s_state = StubState{};
}

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
    s_state.configured = true;
    s_state.configure_count++;
    s_state.sercom = sercom;
    s_state.rx = rx;
    s_state.tx = tx;
    s_state.clock = clock;
    s_state.mode = mode;
    s_state.baud_rate = baud_rate;
    s_state.data_order = data_order;
    s_state.data_bits = data_bits;
    s_state.stop_bits = stop_bits;
    s_state.parity = parity;
}

void UsartHal::sendSync(SercomKind sercom, const U8* data, U32 size) {
    s_state.send_sync_count++;
    s_state.send_sync_sercom = sercom;
    s_state.send_sync_size = size;
    if (data != nullptr && size <= sizeof(s_state.send_sync_data)) {
        std::memcpy(s_state.send_sync_data, data, size);
    }
}

U32 UsartHal::getDataRegisterAddress(SercomKind sercom) {
    return s_state.data_register_address;
}

}  // namespace UsartHardware
}  // namespace Samd21

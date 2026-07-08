// ======================================================================
// \title  UsartDriverHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for USART driver peripheral operations
// ======================================================================

#ifndef Samd21_UsartDriverHardware_HPP
#define Samd21_UsartDriverHardware_HPP

#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriver.hpp"

namespace Samd21 {
namespace UsartHardware {

struct UsartHal {
    //! Configure and enable the SERCOM USART peripheral
    //!
    //! Performs the full §27.6.2.1 initialization sequence (clock gating, GCLK
    //! routing, CTRLA/CTRLB, baud rate) and returns once the peripheral is
    //! enabled. All hardware synchronization waits are bounded to ~1s.
    static void configure(SercomKind sercom,
                          UsartDriver::RxPinOut rx,
                          UsartDriver::TxPinOut tx,
                          UsartDriver::ClockMode clock,
                          UsartDriver::CommunicationMode mode,
                          UsartDriver::BaudRate baud_rate,
                          UsartDriver::DataOrder data_order,
                          UsartDriver::DataBits data_bits,
                          UsartDriver::StopBits stop_bits,
                          UsartDriver::Parity parity);

    //! Synchronously transmit a buffer over the USART (blocking)
    //!
    //! Polls the data register empty flag before each byte. All waits are
    //! bounded to ~1s.
    //! \param sercom SERCOM peripheral to transmit on
    //! \param data Pointer to the bytes to transmit
    //! \param size Number of bytes to transmit
    static void sendSync(SercomKind sercom, const U8* data, U32 size);

    //! Get the address of the USART DATA register
    //!
    //! Used as the DMA source (RX) or destination (TX) address.
    //! \param sercom SERCOM peripheral to query
    //! \return Address of the SERCOM USART DATA register
    static U32 getDataRegisterAddress(SercomKind sercom);
};

//! Test-only hooks into the stub hardware implementation.
//! These are compiled only for native/test builds (not the SAMD21 target) and
//! let unit tests observe the arguments passed to the HAL and control the
//! values it returns.
#ifndef __SAMD21__
//! State captured by the stub hardware on every HAL call.
struct StubState {
    // configure() capture
    bool configured;
    U32 configure_count;
    SercomKind sercom;
    UsartDriver::RxPinOut rx;
    UsartDriver::TxPinOut tx;
    UsartDriver::ClockMode clock;
    UsartDriver::CommunicationMode mode;
    UsartDriver::BaudRate baud_rate;
    UsartDriver::DataOrder data_order;
    UsartDriver::DataBits data_bits;
    UsartDriver::StopBits stop_bits;
    UsartDriver::Parity parity;

    // sendSync() capture
    U32 send_sync_count;
    SercomKind send_sync_sercom;
    U32 send_sync_size;
    U8 send_sync_data[512];

    // getDataRegisterAddress() control
    U32 data_register_address;
};

//! Get the mutable stub state (shared across all HAL calls)
StubState& getStubState();

//! Reset the stub state for a clean test run
void resetStubState();
#endif

}  // namespace UsartHardware
}  // namespace Samd21

#endif

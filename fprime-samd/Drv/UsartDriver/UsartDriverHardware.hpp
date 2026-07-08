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

//! Hardware abstraction layer for USART peripheral operations
//! This isolates all MCU register access so UsartDriver.cpp can build under
//! native unit testing (Linux) by linking the stub implementation instead.
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

}  // namespace UsartHardware
}  // namespace Samd21

#endif

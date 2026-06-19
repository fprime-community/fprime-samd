// ======================================================================
// \title  UsartDriver.cpp
// \author tumbar
// \brief  cpp file for UsartDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/UsartDriver/UsartDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/Types/TriggerSourceEnumAc.hpp"
#include "samd.h"

namespace Samd21 {

static Sercom* getSercomHw(SercomKind sercom);

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UsartDriver ::UsartDriver(const char* const compName) : UsartDriverComponentBase(compName) {}

UsartDriver ::~UsartDriver() {}

void UsartDriver ::configure(SercomKind sercom,
                             RxPinOut rx,
                             TxPinOut tx,
                             ClockMode clock,
                             CommunicationMode mode,
                             BaudRate baud_rate,
                             DataOrder data_order,
                             DataBits data_bits,
                             StopBits stop_bits,
                             Parity parity) {
    // Validate the inputs
    switch (sercom) {
        case SercomKind::SERCOM_0:
        case SercomKind::SERCOM_1:
        case SercomKind::SERCOM_2:
        case SercomKind::SERCOM_3:
        case SercomKind::SERCOM_4:
        case SercomKind::SERCOM_5:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(sercom));
    }

    switch (rx) {
        case RxPinOut::PAD0:
        case RxPinOut::PAD1:
        case RxPinOut::PAD2:
        case RxPinOut::PAD3:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(rx));
    }

    switch (tx) {
        case TxPinOut::PAD0:
        case TxPinOut::PAD1:
        case TxPinOut::PAD2:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(tx));
    }

    switch (clock) {
        case ClockMode::EXTERNAL:
        case ClockMode::INTERNAL:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(clock));
    }

    switch (mode) {
        case CommunicationMode::ASYNC:
        case CommunicationMode::SYNC:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(mode));
    }

    switch (baud_rate) {
        case BaudRate::BAUD_9600:
        case BaudRate::BAUD_19200:
        case BaudRate::BAUD_38400:
        case BaudRate::BAUD_57600:
        case BaudRate::BAUD_115200:
        case BaudRate::BAUD_230400:
        case BaudRate::BAUD_460800:
        case BaudRate::BAUD_921600:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(baud_rate));
    }

    switch (data_order) {
        case DataOrder::MSB_FIRST:
        case DataOrder::LSB_FIRST:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_order));
    }

    switch (data_bits) {
        case DataBits::BITS_8:
        case DataBits::BITS_9:
        case DataBits::BITS_5:
        case DataBits::BITS_6:
        case DataBits::BITS_7:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_bits));
    }

    switch (stop_bits) {
        case StopBits::ONE:
        case StopBits::TWO:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(stop_bits));
    }

    switch (parity) {
        case Parity::NONE:
        case Parity::EVEN:
        case Parity::ODD:
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(parity));
    }

    // Store configuration
    m_sercom = sercom;

    // Get SERCOM hardware register base
    Sercom* sercom_hw = getSercomHw(sercom);
    FW_ASSERT(sercom_hw != nullptr);

    // § 27.6.2.1 Initialization: These registers are enable-protected and can only
    // be written when CTRLA.ENABLE=0

    // Disable USART if currently enabled
    sercom_hw->USART.CTRLA.bit.ENABLE = 0;
    // Wait for synchronization
    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;

    // Step 1: Configure operating mode (internal/external clock)
    // CTRLA.MODE for USART with internal clock = 0x1
    U32 ctrla = SERCOM_USART_CTRLA_MODE(static_cast<U8>(clock));

    // Step 2: Configure communication mode (async/sync)
    if (mode == CommunicationMode::SYNC) {
        ctrla |= SERCOM_USART_CTRLA_CMODE;
    }

    // Step 3: Configure RX pin
    ctrla |= SERCOM_USART_CTRLA_RXPO(static_cast<U8>(rx));

    // Step 4: Configure TX pin
    ctrla |= SERCOM_USART_CTRLA_TXPO(static_cast<U8>(tx));

    // Step 6: Configure data order (LSB/MSB first)
    if (data_order == DataOrder::LSB_FIRST) {
        ctrla |= SERCOM_USART_CTRLA_DORD;
    }

    // Step 7a: Configure frame format (parity mode)
    if (parity != Parity::NONE) {
        ctrla |= SERCOM_USART_CTRLA_FORM(0x1);  // Enable parity
    }

    // Write CTRLA register
    sercom_hw->USART.CTRLA.reg = ctrla;
    while (sercom_hw->USART.SYNCBUSY.reg)
        ;

    // Configure CTRLB register
    U32 ctrlb = 0;

    // Step 5: Configure character size (data bits)
    ctrlb |= SERCOM_USART_CTRLB_CHSIZE(static_cast<U8>(data_bits));

    // Step 7b: Configure parity mode (even/odd)
    if (parity == Parity::ODD) {
        ctrlb |= SERCOM_USART_CTRLB_PMODE;
    }

    // Step 8: Configure stop bits
    if (stop_bits == StopBits::TWO) {
        ctrlb |= SERCOM_USART_CTRLB_SBMODE;
    }

    // Step 10: Enable transmitter and receiver
    ctrlb |= SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_RXEN;

    // Write CTRLB register
    sercom_hw->USART.CTRLB.reg = ctrlb;
    while (sercom_hw->USART.SYNCBUSY.reg)
        ;

    // Step 9: Configure baud rate (only for internal clock)
    if (clock == ClockMode::INTERNAL) {
        U16 baud_value = calculateBaud(baud_rate, mode);
        sercom_hw->USART.BAUD.reg = baud_value;
        while (sercom_hw->USART.SYNCBUSY.reg)
            ;
    }

    // Enable USART
    sercom_hw->USART.CTRLA.bit.ENABLE = 1;
    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;
}

// ----------------------------------------------------------------------
// Helper function implementations
// ----------------------------------------------------------------------

static Sercom* getSercomHw(SercomKind sercom) {
    switch (sercom.e) {
        case SercomKind::SERCOM_0:
            return SERCOM0;
        case SercomKind::SERCOM_1:
            return SERCOM1;
        case SercomKind::SERCOM_2:
            return SERCOM2;
        case SercomKind::SERCOM_3:
            return SERCOM3;
            // Some SAMD21 devices don't have these SERCOM ports
#ifdef SERCOM4
        case SercomKind::SERCOM_4:
            return SERCOM4;
#endif
#ifdef SERCOM5
        case SercomKind::SERCOM_5:
            return SERCOM5;
#endif
        default:
            FW_ASSERT(0, sercom.e);
            return nullptr;
    }
}

U16 UsartDriver ::calculateBaud(BaudRate baud_rate, CommunicationMode mode) {
    const U32 f_ref = F_CPU;
    const U32 f_baud = static_cast<U32>(baud_rate);
    U16 baud_value;

    switch (mode) {
        case CommunicationMode::ASYNC: {
            // Asynchronous Arithmetic mode (§27.6.2.3, Table 26-2)
            // BAUD = 65536 * (1 - S * (f_BAUD / f_ref))
            // where S = 16 (oversampling)
            const U32 S = 16;
            baud_value = 65536 - ((65536 * S * f_baud) / f_ref);
            break;
        }
        case CommunicationMode::SYNC:
            // Synchronous mode (§27.6.2.3, Table 26-2)
            // BAUD = (f_ref / (2 * f_BAUD))
            baud_value = f_ref / (2 * f_baud);

            // Validate range
            FW_ASSERT(baud_value <= 255, baud_value);
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(mode));
    }

    return baud_value;
}

Dma::TriggerSource UsartDriver ::getSercomTxTrigger(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return Dma::TriggerSource::SERCOM0_TX;
        case SercomKind::SERCOM_1:
            return Dma::TriggerSource::SERCOM1_TX;
        case SercomKind::SERCOM_2:
            return Dma::TriggerSource::SERCOM2_TX;
        case SercomKind::SERCOM_3:
            return Dma::TriggerSource::SERCOM3_TX;
        case SercomKind::SERCOM_4:
            return Dma::TriggerSource::SERCOM4_TX;
        case SercomKind::SERCOM_5:
            return Dma::TriggerSource::SERCOM5_TX;
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(sercom));
    }
}

Dma::TriggerSource UsartDriver ::getSercomRxTrigger(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return Dma::TriggerSource::SERCOM0_RX;
        case SercomKind::SERCOM_1:
            return Dma::TriggerSource::SERCOM1_RX;
        case SercomKind::SERCOM_2:
            return Dma::TriggerSource::SERCOM2_RX;
        case SercomKind::SERCOM_3:
            return Dma::TriggerSource::SERCOM3_RX;
        case SercomKind::SERCOM_4:
            return Dma::TriggerSource::SERCOM4_RX;
        case SercomKind::SERCOM_5:
            return Dma::TriggerSource::SERCOM5_RX;
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(sercom));
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void UsartDriver ::dmaReplyIn_handler(FwIndexType portNum, const Samd21::Dma::Reply& reply) {
    // TODO
}

void UsartDriver ::recvReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // TODO
}

void UsartDriver ::send_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // TODO
}

}  // namespace Samd21

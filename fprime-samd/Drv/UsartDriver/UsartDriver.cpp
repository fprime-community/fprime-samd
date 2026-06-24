// ======================================================================
// \title  UsartDriver.cpp
// \author tumbar
// \brief  cpp file for UsartDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/UsartDriver/UsartDriver.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SuccessEnumAc.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwIndexTypeAliasAc.h"
#include "config/FwSizeTypeAliasAc.h"
#include "fprime-samd/Drv/Types/PriorityEnumAc.hpp"
#include "fprime-samd/Drv/Types/TriggerSourceEnumAc.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriver_DmaChannelEnumAc.hpp"
#include "samd.h"

namespace Samd21 {

static Sercom* getSercomHw(SercomKind sercom);

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UsartDriver ::UsartDriver(const char* const compName)
    : UsartDriverComponentBase(compName), m_active_rx(RxDmaBufferID::INVALID), m_configured(false) {}

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
                             Parity parity,
                             FwSizeType rx_buffer_size,
                             U16 rx_dog_cnt) {
    FW_ASSERT(!this->m_configured, static_cast<FwAssertArgType>(sercom));
    FW_ASSERT(rx_dog_cnt > 0, static_cast<FwAssertArgType>(sercom));

    // Get SERCOM hardware register base
    Sercom* sercom_hw = getSercomHw(sercom);
    FW_ASSERT(sercom_hw != nullptr, sercom);

    // Store configuration
    m_sercom = sercom;

    // Build CTRLA and CTRLB registers per §27.6.2.1 initialization sequence
    // Steps 1-4, 6, 7a configure CTRLA; steps 5, 7b, 8, 10 configure CTRLB
    SERCOM_USART_CTRLA_Type ctrla = {.reg = 0};
    SERCOM_USART_CTRLB_Type ctrlb = {.reg = 0};

    // Step 1: Select operating mode
    switch (clock) {
        case ClockMode::EXTERNAL:
            ctrla.bit.MODE = SERCOM_USART_CTRLA_MODE_USART_EXT_CLK_Val;
            break;
        case ClockMode::INTERNAL:
            ctrla.bit.MODE = SERCOM_USART_CTRLA_MODE_USART_INT_CLK_Val;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(clock));
    }

    // Step 2: Select communication mode
    switch (mode) {
        case CommunicationMode::ASYNC:
            ctrla.bit.CMODE = 0;  // Asynchronous
            break;
        case CommunicationMode::SYNC:
            ctrla.bit.CMODE = 1;  // Synchronous
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(mode));
    }

    // Step 3: Select RX pin
    switch (rx) {
        case RxPinOut::PAD0:
            ctrla.bit.RXPO = 0;  // RxD on PAD[0]
            break;
        case RxPinOut::PAD1:
            ctrla.bit.RXPO = 1;  // RxD on PAD[1]
            break;
        case RxPinOut::PAD2:
            ctrla.bit.RXPO = 2;  // RxD on PAD[2]
            break;
        case RxPinOut::PAD3:
            ctrla.bit.RXPO = 3;  // RxD on PAD[3]
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(rx));
    }

    // Step 4: Select TX pin
    switch (tx) {
        case TxPinOut::PAD0:
            ctrla.bit.TXPO = 0;  // TxD on PAD[0], XCK on PAD[1]
            break;
        case TxPinOut::PAD1:
            ctrla.bit.TXPO = 1;  // TxD on PAD[2], XCK on PAD[3]
            break;
        case TxPinOut::PAD2:
            ctrla.bit.TXPO = 2;  // TxD on PAD[0], RTS on PAD[2], CTS on PAD[3]
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(tx));
    }

    // Step 5: Configure character size
    switch (data_bits) {
        case DataBits::BITS_5:
            ctrlb.bit.CHSIZE = 5;  // 5 data bits
            break;
        case DataBits::BITS_6:
            ctrlb.bit.CHSIZE = 6;  // 6 data bits
            break;
        case DataBits::BITS_7:
            ctrlb.bit.CHSIZE = 7;  // 7 data bits
            break;
        case DataBits::BITS_8:
            ctrlb.bit.CHSIZE = 0;  // 8 data bits (default)
            break;
        case DataBits::BITS_9:
            ctrlb.bit.CHSIZE = 1;  // 9 data bits
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_bits));
    }

    // Step 6: Set data order
    switch (data_order) {
        case DataOrder::MSB_FIRST:
            ctrla.bit.DORD = 0;  // MSB transmitted first
            break;
        case DataOrder::LSB_FIRST:
            ctrla.bit.DORD = 1;  // LSB transmitted first
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_order));
    }

    // Step 7a: Enable parity mode
    switch (parity) {
        case Parity::NONE:
            ctrla.bit.FORM = 0;  // USART frame without parity
            break;
        case Parity::EVEN:
        case Parity::ODD:
            ctrla.bit.FORM = 1;  // USART frame with parity
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(parity));
    }

    // Step 7b: Configure parity mode
    switch (parity) {
        case Parity::NONE:
        case Parity::EVEN:
            ctrlb.bit.PMODE = 0;  // Even parity
            break;
        case Parity::ODD:
            ctrlb.bit.PMODE = 1;  // Odd parity
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(parity));
    }

    // Step 8: Configure stop bits (CTRLB.SBMODE: 0=1 stop bit, 1=2 stop bits)
    switch (stop_bits) {
        case StopBits::ONE:
            ctrlb.bit.SBMODE = 0;  // 1 stop bit
            break;
        case StopBits::TWO:
            ctrlb.bit.SBMODE = 1;  // 2 stop bits
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(stop_bits));
    }

    // Step 10: Enable transmitter and receiver (CTRLB.TXEN, CTRLB.RXEN)
    ctrlb.bit.TXEN = 1;
    ctrlb.bit.RXEN = 1;

    // § 27.6.2.1 Initialization: These registers are enable-protected and can only
    // be written when CTRLA.ENABLE=0

    // Disable USART if currently enabled
    sercom_hw->USART.CTRLA.bit.ENABLE = 0;
    // Wait for synchronization
    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;

    // Write CTRLA register (steps 1-4, 6, 7a)
    sercom_hw->USART.CTRLA.reg = ctrla.reg;
    while (sercom_hw->USART.SYNCBUSY.reg)
        ;

    // Write CTRLB register (steps 5, 7b, 8, 10)
    sercom_hw->USART.CTRLB.reg = ctrlb.reg;
    while (sercom_hw->USART.SYNCBUSY.reg)
        ;

    // Step 9: Configure baud rate (only for internal clock)
    if (clock == ClockMode::INTERNAL) {
        U16 baud_value = calculateBaud(baud_rate, mode);
        sercom_hw->USART.BAUD.reg = baud_value;
        while (sercom_hw->USART.SYNCBUSY.reg)
            ;
    }

    // Allocate buffers for the Rx
    this->m_rx[0] = ThinBuffer(this->allocate_out(0, rx_buffer_size));
    this->m_rx[1] = ThinBuffer(this->allocate_out(0, rx_buffer_size));

    // Send these buffers to the DMA
    this->dmaQueueRxSend(this->m_rx[0]);
    this->dmaQueueRxSend(this->m_rx[1]);
    this->dmaRxCircular_out(0);

    // Initialize the RX state
    this->m_rx_state[0] = RxDmaBufferState::DMA;
    this->m_rx_state[1] = RxDmaBufferState::DMA_WAITING;
    this->m_active_rx = RxDmaBufferID::A;
    this->m_rx_dog = rx_dog_cnt;
    this->m_rx_dog_reset = rx_dog_cnt;

    this->m_configured = true;

    // Enable USART
    sercom_hw->USART.CTRLA.bit.ENABLE = 1;
    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;

    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);
    }
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

void UsartDriver ::schedIn_handler(FwIndexType portNum, U32 context) {
    FW_ASSERT(this->m_rx_dog > 0);
    this->m_rx_dog -= 1;

    if (this->m_rx_dog == 0) {
        // We have tripped the idle watchdog!

        // Read the active Rx transfer
        auto currentRx = this->dmaRxPopCurrent_out(0);

        // Notify our queue of the current Rx status
        this->m_queue.enqueue(Samd21::UsartDriver::Signal{
            .kind = SignalKind::RX_BUFFER_DONE, .rx = m_active_rx, .rx_bytes_remaining = currentRx.get_btcnt()});
    }
}

void UsartDriver ::cycleIn_handler(FwIndexType portNum, U32 context) {
    // Unload the queue
    while (!this->m_queue.isEmpty()) {
        Signal signal;
        auto status = this->m_queue.dequeue(signal);
        FW_ASSERT(status == Fw::Success::SUCCESS, status);

        ThinBuffer thinBuffer;
        Fw::Buffer thickBuffer;
        Fw::Success bufferStatus;

        switch (signal.kind) {
            case SignalKind::TX_BUFFER_OK:
                bufferStatus = this->m_tx_queue.dequeue(thinBuffer);
                FW_ASSERT(bufferStatus == Fw::Success::SUCCESS);
                thickBuffer = thinBuffer.getBuffer();

                this->sendReturnOut_out(0, thickBuffer, Drv::ByteStreamStatus::OP_OK);
                break;
            case SignalKind::TX_CHANNEL_ERROR:
                // A tx channel error indicates the entire TX DMA channel needs to be restarted
                // We need to dump all the bending buffers on the queue
                while (!this->m_tx_queue.isEmpty()) {
                    bufferStatus = this->m_tx_queue.dequeue(thinBuffer);
                    FW_ASSERT(bufferStatus == Fw::Success::SUCCESS);
                    thickBuffer = thinBuffer.getBuffer();
                    this->sendReturnOut_out(0, thickBuffer, Drv::ByteStreamStatus::OTHER_ERROR);
                }

                break;
            case SignalKind::RX_BUFFER_DONE: {
                // Stroke the RX watchdog
                this->m_rx_dog = this->m_rx_dog_reset;

                switch (signal.rx) {
                    case RxDmaBufferID::A:
                        // Make sure our expected state matches
                        FW_ASSERT(this->m_rx_state[0] == RxDmaBufferState::DMA,
                                  static_cast<FwAssertArgType>(this->m_rx_state[0]));
                        FW_ASSERT(this->m_rx_state[1] == RxDmaBufferState::DMA_WAITING,
                                  static_cast<FwAssertArgType>(this->m_rx_state[1]));

                        // Update all the states to represent the new overall state
                        this->m_active_rx = RxDmaBufferID::B;
                        this->m_rx_state[1] = RxDmaBufferState::DMA;
                        this->m_rx_state[0] = RxDmaBufferState::IN_USE;
                        thickBuffer = this->m_rx[0].getBuffer();
                        break;
                    case RxDmaBufferID::B:
                        // Make sure our expected state matches
                        FW_ASSERT(this->m_rx_state[1] == RxDmaBufferState::DMA,
                                  static_cast<FwAssertArgType>(this->m_rx_state[1]));
                        FW_ASSERT(this->m_rx_state[0] == RxDmaBufferState::DMA_WAITING,
                                  static_cast<FwAssertArgType>(this->m_rx_state[0]));

                        // Update all the states to represent the new overall state
                        this->m_active_rx = RxDmaBufferID::A;
                        this->m_rx_state[0] = RxDmaBufferState::DMA;
                        this->m_rx_state[1] = RxDmaBufferState::IN_USE;
                        thickBuffer = this->m_rx[1].getBuffer();
                        break;
                    case RxDmaBufferID::INVALID:
                        FW_ASSERT(false, static_cast<FwAssertArgType>(signal.rx));
                }

                FW_ASSERT(thickBuffer.getSize() >= signal.rx_bytes_remaining);
                thickBuffer.setSize(thickBuffer.getSize() - signal.rx_bytes_remaining);

                // Send out the data we received to downstream processes
                if (thickBuffer.getSize() > 0) {
                    this->recv_out(0, thickBuffer, Drv::ByteStreamStatus::OP_OK);
                } else {
                    // We have not received any data but we tripped the watchdog.
                    // We can drop this signal
                }

                break;
            }
            case SignalKind::RX_CHANNEL_ERROR:
                // TODO(tumbar) What do we do here? Give the buffer back to DMA?
                FW_ASSERT(false);
                break;
            default:
                FW_ASSERT(false, static_cast<FwAssertArgType>(signal.kind));
        }
    }
}

void UsartDriver ::dmaReplyIn_handler(FwIndexType portNum, const Samd21::Dma::Reply& reply) {
    switch (portNum) {
        case UsartDriver_DmaChannel::TX:
            this->dmaReplyTxIsr(reply);
            break;
        case UsartDriver_DmaChannel::RX:
            this->dmaReplyRxIsr(reply);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(portNum));
    }
}

void UsartDriver ::recvReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    // The 'other' buffer (i.e. non currently DMA-ing buffer) should be in-use.
    // When we get the buffer back we
    switch (this->m_active_rx) {
        case RxDmaBufferID::A:
            FW_ASSERT(this->m_rx_state[1] == RxDmaBufferState::IN_USE,
                      static_cast<FwAssertArgType>(this->m_rx_state[1]));
            this->m_rx_state[1] = RxDmaBufferState::DMA_WAITING;
            break;
        case RxDmaBufferID::B:
            FW_ASSERT(this->m_rx_state[0] == RxDmaBufferState::IN_USE,
                      static_cast<FwAssertArgType>(this->m_rx_state[0]));
            this->m_rx_state[0] = RxDmaBufferState::DMA_WAITING;
            break;
        case RxDmaBufferID::INVALID:
            break;
    }
}

void UsartDriver ::send_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    auto status = this->m_tx_queue.enqueue(ThinBuffer(fwBuffer));
    if (status == Fw::Success::SUCCESS) {
        // This job has been added to the queue, send it to the DMA
        this->dmaQueueOut_out(
            UsartDriver_DmaChannel::TX, getSercomTxTrigger(m_sercom), Dma::TransactionType::BEAT,
            Dma::Priority::PRIORITY_0, reinterpret_cast<U32>(fwBuffer.getData()),
            reinterpret_cast<U32>(&getSercomHw(m_sercom)->USART.DATA), fwBuffer.getSize(), Dma::BeatSize::BYTE,
            /* increment source */ true,
            /* incrementDestination */ false, Dma::AddressIncrementStepSize::SIZE_1, Dma::StepSelection::SOURCE);
    } else {
        // We could not add this buffer to the queue
        // Reply back to the producer with a "retry"
        this->sendReturnOut_out(0, fwBuffer, Drv::ByteStreamStatus::SEND_RETRY);
    }
}

void UsartDriver ::dmaQueueRxSend(const ThinBuffer& buffer) {
    this->dmaQueueOut_out(UsartDriver_DmaChannel::RX, getSercomRxTrigger(m_sercom), Dma::TransactionType::BEAT,
                          Dma::Priority::PRIORITY_0, reinterpret_cast<U32>(&getSercomHw(m_sercom)->USART.DATA),
                          reinterpret_cast<U32>(buffer.getData()), buffer.getSize(), Dma::BeatSize::BYTE,
                          /* increment source */ false,
                          /* incrementDestination */ true, Dma::AddressIncrementStepSize::SIZE_1,
                          Dma::StepSelection::DESTINATION);
}

void UsartDriver ::dmaReplyTxIsr(const Samd21::Dma::Reply& reply) {
    Fw::Success status;
    switch (reply.get_status()) {
        case Dma::Status::OK:
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::TX_BUFFER_OK,
                .rx = RxDmaBufferID::INVALID,
                .rx_bytes_remaining = 0,
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        case Dma::Status::BUS_ERROR:
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::TX_CHANNEL_ERROR,
                .rx = RxDmaBufferID::INVALID,
                .rx_bytes_remaining = 0,
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(reply.get_status()));
    }
}

void UsartDriver ::dmaReplyRxIsr(const Samd21::Dma::Reply& reply) {
    Fw::Success status;
    switch (reply.get_status()) {
        case Dma::Status::OK: {
            // Make sure remaining bytes will fit in our storage
            FW_ASSERT(reply.get_remainingBytes() <= 0xFFFF, reply.get_remainingBytes());
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::RX_BUFFER_DONE,
                .rx = this->m_active_rx,
                .rx_bytes_remaining = static_cast<U16>(reply.get_remainingBytes()),
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
        } break;
        case Dma::Status::BUS_ERROR:
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::RX_CHANNEL_ERROR,
                .rx = RxDmaBufferID::INVALID,
                .rx_bytes_remaining = 0,
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(reply.get_status()));
    }
}

}  // namespace Samd21

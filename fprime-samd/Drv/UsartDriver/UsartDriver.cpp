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
#include "config/UsartDriverConfig.hpp"
#include "fprime-samd/Drv/Types/PriorityEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
#include "fprime-samd/Drv/Types/TriggerSourceEnumAc.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriver_DmaChannelEnumAc.hpp"
#include "fprime-samd/config/UsartDriverConfig.hpp"
#include "samd.h"

namespace Samd21 {

static Sercom* getSercomHw(SercomKind sercom);

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UsartDriver ::UsartDriver(const char* const compName)
    : UsartDriverComponentBase(compName),
      m_sercom(SercomKind::SERCOM_0),
      m_active_rx(RxDmaBufferID::INVALID),
      m_active_processed(0),
      m_configured(false),
      m_rx_dog(0),
      m_rx_dog_reset(0) {}

UsartDriver ::~UsartDriver() = default;

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
                             U16 rx_dog_cnt) {
    FW_ASSERT(!this->m_configured, static_cast<FwAssertArgType>(sercom));
    FW_ASSERT(rx_dog_cnt > 0, static_cast<FwAssertArgType>(sercom));

    // Get SERCOM hardware register base
    Sercom* sercom_hw = getSercomHw(sercom);
    FW_ASSERT(sercom_hw != nullptr, sercom);

    // Store configuration
    m_sercom = sercom;

    // Enable SERCOM peripheral clock (APBC bus)
    // Per §8.6 PM – Power Manager and datasheet Table 14-2
    switch (sercom.e) {
        case SercomKind::SERCOM_0:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM0;
            break;
        case SercomKind::SERCOM_1:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM1;
            break;
        case SercomKind::SERCOM_2:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM2;
            break;
        case SercomKind::SERCOM_3:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3;
            break;
#ifdef SERCOM4
        case SercomKind::SERCOM_4:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM4;
            break;
#endif
#ifdef SERCOM5
        case SercomKind::SERCOM_5:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;
            break;
#endif
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(sercom));
    }

    while (GCLK->STATUS.bit.SYNCBUSY)
        ;

    // Assign Generic Clock Generator 0 (48MHz) to all sercom
    // Per §14.8.3 GCLK_CLKCTRL – Generic Clock Control
    U8 gclk_id = static_cast<U8>(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val) + sercom.e;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id) |
                        GCLK_CLKCTRL_GEN_GCLK0 |  // Use Generic Clock Generator 0 (48MHz main clock)
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY)
        ;

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
        case TxPinOut::PAD2_XCK_PAD3:
            ctrla.bit.TXPO = 1;  // TxD on PAD[2], XCK on PAD[3]
            break;
        case TxPinOut::PAD0_RTS_PAD2_CTS_PAD3:
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

    ctrla.bit.SAMPR = 0;
    ctrlb.bit.RXEN = 1;
    ctrlb.bit.TXEN = 1;

    // § 27.6.2.1 Initialization: These registers are enable-protected and can only
    // be written when CTRLA.ENABLE=0

    // Disable USART if currently enabled
    sercom_hw->USART.CTRLA.bit.ENABLE = 0;
    // Wait for synchronization
    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;

    sercom_hw->USART.CTRLA.reg |= SERCOM_USART_CTRLA_SWRST;
    while (sercom_hw->USART.SYNCBUSY.bit.SWRST)
        ;

    // Write CTRLA register (steps 1-4, 6, 7a)
    sercom_hw->USART.CTRLA.reg = ctrla.reg;

    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;

    // Write CTRLB register (steps 5, 7b, 8)
    sercom_hw->USART.CTRLB.reg = ctrlb.reg;
    while (sercom_hw->USART.SYNCBUSY.bit.CTRLB)
        ;

    // Step 9: Configure baud rate (only for internal clock)
    if (clock == ClockMode::INTERNAL) {
        U16 baud_value = calculateBaud(baud_rate, mode);
        sercom_hw->USART.BAUD.reg = baud_value;
    }

    // Enable USART
    sercom_hw->USART.CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
    while (sercom_hw->USART.SYNCBUSY.bit.ENABLE)
        ;

    // Wait for the Tx and Rx lines to be enabled
    while (sercom_hw->USART.SYNCBUSY.bit.CTRLB)
        ;

    // Initialize the RX state
    this->m_active_rx = RxDmaBufferID::A;
    this->m_active_processed = 0;
    this->m_rx_dog = rx_dog_cnt;
    this->m_rx_dog_reset = rx_dog_cnt;

    this->m_configured = true;

    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);
    }

    if (this->isConnected_dmaQueueOut_OutputPort(UsartDriver_DmaChannel::RX)) {
        // Send these buffers to the DMA
        this->dmaQueueRxSend(ThinBuffer(this->m_rx[0].data, USART_RX_BUFFER_SIZE));
        this->dmaQueueRxSend(ThinBuffer(this->m_rx[1].data, USART_RX_BUFFER_SIZE));
        this->dmaRxCircular_out(0);
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
            FW_ASSERT(false, sercom.e);
            return nullptr;
    }
}

U16 UsartDriver ::calculateBaud(BaudRate baud_rate, CommunicationMode mode) {
    const U64 f_ref = F_CPU;
    const U32 f_baud = static_cast<U32>(baud_rate);
    U16 baud_value;

    switch (mode) {
        case CommunicationMode::ASYNC: {
            // Asynchronous Arithmetic mode (§27.6.2.3, Table 26-2)
            // BAUD = 65536 * (1 - S * (f_BAUD / f_ref))
            // where S = 16 (oversampling)
            const U32 S = 16;
            U64 top = 65536ULL * S * static_cast<U64>(f_baud);
            baud_value = 65536 - static_cast<U64>(top / f_ref);
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
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(sercom));
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void UsartDriver ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_configured) {
        FW_ASSERT(this->m_rx_dog > 0);
        this->m_rx_dog -= 1;

        if (this->m_rx_dog == 0) {
            // We have tripped the idle watchdog!

            // Read the active Rx transfer
            auto currentRx = this->dmaRxRead_out(0);

            // Notify our queue of the current Rx status
            auto status = this->m_queue.enqueue(
                Signal{.kind = SignalKind::RX_BUFFER_PARTIAL, .rx_bytes_remaining = currentRx.get_btcnt()});
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            // The watchdog is acknowledged in the active loop
        }
    }
}

void UsartDriver ::activeIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_configured) {
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
                case SignalKind::RX_BUFFER_DONE:
                case SignalKind::RX_BUFFER_PARTIAL:
                    // Stroke the RX watchdog
                    this->m_rx_dog = this->m_rx_dog_reset;

                    // Make sure the DMA doesn't give us more bytes than we expect
                    FW_ASSERT(this->m_active_processed + signal.rx_bytes_remaining <= USART_RX_BUFFER_SIZE,
                              this->m_sercom, this->m_active_processed, signal.rx_bytes_remaining);

                    // Wrap the current DMA buffer in a Fw::Buffer
                    switch (this->m_active_rx) {
                        case RxDmaBufferID::A:
                            thickBuffer =
                                Fw::Buffer(this->m_rx[0].data + this->m_active_processed,
                                           USART_RX_BUFFER_SIZE - signal.rx_bytes_remaining - this->m_active_processed);
                            break;
                        case RxDmaBufferID::B:
                            thickBuffer =
                                Fw::Buffer(this->m_rx[1].data + this->m_active_processed,
                                           USART_RX_BUFFER_SIZE - signal.rx_bytes_remaining - this->m_active_processed);
                            break;
                        case RxDmaBufferID::INVALID:
                            break;
                    }

                    if (thickBuffer.getSize() > 0) {
                        this->log_DIAGNOSTIC_Rx(static_cast<U8>(this->m_active_rx), thickBuffer.getSize());
                    }

                    switch (signal.kind) {
                        case SignalKind::RX_BUFFER_PARTIAL:
                            // We got part of a buffer, track the amount of data we received
                            this->m_active_processed += thickBuffer.getSize();
                            break;
                        case SignalKind::RX_BUFFER_DONE:
                            // We got the entire buffer, we need to tick the state machine
                            switch (this->m_active_rx) {
                                case RxDmaBufferID::A:
                                    // Update all the states to represent the new overall state
                                    this->m_active_rx = RxDmaBufferID::B;
                                    this->m_active_processed = 0;
                                    break;
                                case RxDmaBufferID::B:
                                    // Update all the states to represent the new overall state
                                    this->m_active_rx = RxDmaBufferID::A;
                                    this->m_active_processed = 0;
                                    break;
                                case RxDmaBufferID::INVALID:
                                    FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_active_rx));
                            }
                            break;
                        default:
                            FW_ASSERT(false);  // unreachable
                    }

                    // Send the data we received to downstream uplink
                    if (thickBuffer.getSize() > 0) {
                        if (this->isConnected_recv_OutputPort(0)) {
                            this->recv_out(0, thickBuffer, Drv::ByteStreamStatus::OP_OK);
                        }
                    }

                    break;
                case SignalKind::RX_CHANNEL_ERROR:
                    // A bus error on the DMA occured, we have no way of handling this
                    FW_ASSERT(false, this->m_sercom, signal.rx_bytes_remaining);
                    break;
                default:
                    FW_ASSERT(false, static_cast<FwAssertArgType>(signal.kind));
            }
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
    // The downstream process gave us our buffer back
    // These buffers are always in the DMA, this is a no-op
    // Do we need overrun detection here?
}

void UsartDriver ::send_handler(FwIndexType portNum, Fw::Buffer& fwBuffer) {
    FW_ASSERT(this->m_configured);

    auto status = this->m_tx_queue.enqueue(ThinBuffer(fwBuffer));
    if (status == Fw::Success::SUCCESS) {
        // This job has been added to the queue, send it to the DMA
        // Note: Use DATA.reg to get the actual register address, not the structure address
        this->dmaQueueOut_out(
            UsartDriver_DmaChannel::TX, getSercomTxTrigger(m_sercom), Dma::TransactionType::BEAT,
            Dma::Priority::PRIORITY_0, reinterpret_cast<U32>(fwBuffer.getData()),
            reinterpret_cast<U32>(&getSercomHw(m_sercom)->USART.DATA.reg), fwBuffer.getSize(), Dma::BeatSize::BYTE,
            /* increment source */ true,
            /* incrementDestination */ false, Dma::AddressIncrementStepSize::SIZE_1, Dma::StepSelection::SOURCE);
    } else {
        // We could not add this buffer to the queue
        // Reply back to the producer with a "retry"
        this->sendReturnOut_out(0, fwBuffer, Drv::ByteStreamStatus::SEND_RETRY);
    }
}

Drv::ByteStreamStatus UsartDriver ::sendSync_handler(FwIndexType portNum, Fw::Buffer& sendBuffer) {
    auto sercom_hw = getSercomHw(this->m_sercom);
    for (U32 i = 0; i < sendBuffer.getSize(); i++) {
        while (!sercom_hw->USART.INTFLAG.bit.DRE)
            ;

        sercom_hw->USART.DATA.reg = sendBuffer.getData()[i];
    }

    while (!sercom_hw->USART.INTFLAG.bit.DRE)
        ;

    return Drv::ByteStreamStatus::OP_OK;
}

void UsartDriver ::dmaQueueRxSend(const ThinBuffer& buffer) {
    // Note: Use DATA.reg to get the actual register address, not the structure address
    this->dmaQueueOut_out(UsartDriver_DmaChannel::RX, getSercomRxTrigger(m_sercom), Dma::TransactionType::BEAT,
                          Dma::Priority::PRIORITY_0, reinterpret_cast<U32>(&getSercomHw(m_sercom)->USART.DATA.reg),
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
                .rx_bytes_remaining = 0,
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        case Dma::Status::BUS_ERROR:
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::TX_CHANNEL_ERROR,
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
        case Dma::Status::OK:
            // Make sure remaining bytes will fit in our storage
            FW_ASSERT(reply.get_remainingBytes() <= 0xFFFF, reply.get_remainingBytes());
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::RX_BUFFER_DONE,
                .rx_bytes_remaining = static_cast<U16>(reply.get_remainingBytes()),
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        case Dma::Status::BUS_ERROR:
            status = this->m_queue.enqueue(Signal{
                .kind = SignalKind::RX_CHANNEL_ERROR,
                .rx_bytes_remaining = static_cast<U16>(reply.get_remainingBytes()),
            });
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(reply.get_status()));
    }
}

}  // namespace Samd21

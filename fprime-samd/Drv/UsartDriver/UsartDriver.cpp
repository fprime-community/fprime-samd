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
#include "fprime-samd/Drv/Types/CriticalSection.hpp"
#include "fprime-samd/Drv/Types/PriorityEnumAc.hpp"
#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriverHardware.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriver_DmaChannelEnumAc.hpp"
#include "fprime-samd/config/UsartDriverConfig.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

UsartDriver ::UsartDriver(const char* const compName)
    : UsartDriverComponentBase(compName),
      m_sercom(SercomKind::SERCOM_0),
      m_active_rx(RxDmaBufferID::INVALID),
      m_configured(false),
      m_active_processed(0),
      m_rxOverflows(0),
      m_schedSkips(0),
      m_rx_activity(0),
      m_last_sched_activity(0),
      m_rxBytes(0),
      m_txBytes(0) {}

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
                             Parity parity) {
    FW_ASSERT(!this->m_configured, sercom.e);

    // Store configuration
    m_sercom = sercom;

    // Configure and enable the SERCOM USART peripheral.
    UsartHardware::UsartHal::configure(sercom, rx, tx, clock, mode, baud_rate, data_order, data_bits, stop_bits,
                                       parity);

    // Initialize the RX state
    this->m_active_rx = RxDmaBufferID::A;
    this->m_active_processed = 0;

    this->m_configured = true;

    if (this->isConnected_ready_OutputPort(0)) {
        this->ready_out(0);
    }

    // Queue the Rx buffer to get the DMA to start reading from the uart
    if (this->isConnected_dmaQueueOut_OutputPort(UsartDriver_DmaChannel::RX)) {
        // Send these buffers to the DMA
        this->dmaQueueRxSend(ThinBuffer(this->m_rx[0].data, USART_RX_BUFFER_SIZE));
        this->dmaQueueRxSend(ThinBuffer(this->m_rx[1].data, USART_RX_BUFFER_SIZE));
        this->dmaRxCircular_out(0);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void UsartDriver ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_configured) {
        // Check the SERCOM RX hardware overflow flag. This is a plain register
        // read (no channel suspend) and is safe to do at any RX rate; it tells us
        // whether the peripheral silently dropped any bytes since the last tick.
        if (UsartHardware::UsartHal::checkAndClearRxOverflow(this->m_sercom)) {
            this->m_rxOverflows++;
        }

        // High-RX watchdog. dmaReplyRxIsr bumps m_rx_activity on every full-buffer
        // RX completion. If it advanced since the previous tick, RX is actively
        // streaming and the partial read below
        const U32 activity = this->m_rx_activity;
        const bool rxBusy = (activity != this->m_last_sched_activity);
        this->m_last_sched_activity = activity;

        if (rxBusy) {
            this->m_schedSkips++;
        } else {
            // RX is idle -- safe to suspend and read the in-progress transfer.
            auto currentRx = this->dmaRxRead_out(0);

            // Absolute count of bytes filled in the current DMA buffer so far
            auto filled = static_cast<U16>(USART_RX_BUFFER_SIZE - currentRx.get_btcnt());

            // Only notify if new data has arrived since we last processed this buffer.
            // Signals carry the absolute high-water mark (not a delta) so that a PARTIAL and
            // a concurrently-enqueued DONE/PARTIAL cannot double-count the same bytes.
            if (filled > this->m_active_processed) {
                // Notify our queue of the current Rx status.
                // m_queue is shared with the DMA ISR (dmaReplyIn), so the enqueue must be
                // protected from a concurrent enqueue happening inside the ISR.
                Fw::Success status;
                {
                    CriticalSection cs;
                    status = this->m_queue.enqueue(Signal(SignalKind::RX_BUFFER_PARTIAL, filled));
                }
                FW_ASSERT(status == Fw::Success::SUCCESS, status);
            }
        }

        // Emit diagnostics on the sched tick (no added TX load beyond telemetry).
        auto time = getTime();
        this->tlmWrite_rxOverflows(this->m_rxOverflows, time);
        this->tlmWrite_rxBytes(this->m_rxBytes, time);
        this->tlmWrite_txBytes(this->m_txBytes, time);
    }
}

void UsartDriver ::activeIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_configured) {
        // Unload the queue
        for (;;) {
            Signal signal;
            Fw::Success status;

            // Dequeue an item off the queue
            {
                CriticalSection cs;
                if (this->m_queue.isEmpty()) {
                    // No more items
                    break;
                } else {
                    status = this->m_queue.dequeue(signal);
                }
            }

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
                case SignalKind::RX_BUFFER_DONE:
                case SignalKind::RX_BUFFER_PARTIAL: {
                    FW_ASSERT(signal.rx_bytes <= USART_RX_BUFFER_SIZE, this->m_sercom, signal.rx_bytes);
                    FW_ASSERT(signal.rx_bytes >= this->m_active_processed, this->m_sercom, this->m_active_processed,
                              signal.rx_bytes);

                    U16 newBytes = signal.rx_bytes - this->m_active_processed;
                    // Wrap the newly received region of the current DMA buffer in a Fw::Buffer
                    switch (this->m_active_rx) {
                        case RxDmaBufferID::A:
                            thickBuffer = Fw::Buffer(this->m_rx[0].data + this->m_active_processed, newBytes);
                            break;
                        case RxDmaBufferID::B:
                            thickBuffer = Fw::Buffer(this->m_rx[1].data + this->m_active_processed, newBytes);
                            break;
                        case RxDmaBufferID::INVALID:
                            break;
                        default:
                            FW_ASSERT(false);
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
                                default:
                                    FW_ASSERT(false);
                            }
                            break;
                        default:
                            FW_ASSERT(false);  // unreachable
                    }

                    // Send the data we received to downstream uplink
                    if (thickBuffer.getSize() > 0) {
                        this->m_rxBytes += static_cast<U32>(thickBuffer.getSize());
                        if (this->isConnected_recv_OutputPort(0)) {
                            this->recv_out(0, thickBuffer, Drv::ByteStreamStatus::OP_OK);
                        }
                    }

                    break;
                }
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
            FW_ASSERT(false, static_cast<FwAssertArgType>(portNum));
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
        this->m_txBytes += static_cast<U32>(fwBuffer.getSize());
        // This job has been added to the queue, send it to the DMA
        // Note: Use DATA.reg to get the actual register address, not the structure address
        this->dmaQueueOut_out(
            UsartDriver_DmaChannel::TX, SercomUtil::txDmaTrigger(m_sercom), Dma::TransactionType::BEAT,
            Dma::Priority::PRIORITY_0, static_cast<U32>(reinterpret_cast<uintptr_t>(fwBuffer.getData())),
            UsartHardware::UsartHal::getDataRegisterAddress(m_sercom), fwBuffer.getSize(), Dma::BeatSize::BYTE,
            /* increment source */ true,
            /* incrementDestination */ false, Dma::AddressIncrementStepSize::SIZE_1, Dma::StepSelection::SOURCE);
    } else {
        // We could not add this buffer to the queue
        // Reply back to the producer with a "retry"
        this->sendReturnOut_out(0, fwBuffer, Drv::ByteStreamStatus::SEND_RETRY);
    }
}

Drv::ByteStreamStatus UsartDriver ::sendSync_handler(FwIndexType portNum, Fw::Buffer& sendBuffer) {
    UsartHardware::UsartHal::sendSync(this->m_sercom, sendBuffer.getData(), sendBuffer.getSize());
    return Drv::ByteStreamStatus::OP_OK;
}

void UsartDriver ::dmaQueueRxSend(const ThinBuffer& buffer) {
    this->dmaQueueOut_out(
        UsartDriver_DmaChannel::RX, SercomUtil::rxDmaTrigger(m_sercom), Dma::TransactionType::BEAT,
        Dma::Priority::PRIORITY_0, UsartHardware::UsartHal::getDataRegisterAddress(m_sercom),
        static_cast<U32>(reinterpret_cast<uintptr_t>(buffer.getData())), buffer.getSize(), Dma::BeatSize::BYTE,
        /* increment source */ false,
        /* incrementDestination */ true, Dma::AddressIncrementStepSize::SIZE_1, Dma::StepSelection::DESTINATION);
}

void UsartDriver ::dmaReplyTxIsr(const Samd21::Dma::Reply& reply) {
    Fw::Success status;
    switch (reply.get_status()) {
        case Dma::Status::OK: {
            CriticalSection cs;
            status = this->m_queue.enqueue(Signal(SignalKind::TX_BUFFER_OK, 0));
        }
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        case Dma::Status::BUS_ERROR:
            // We tried to transmit from a buffer on an invalid address
            FW_ASSERT(false, this->m_sercom);
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(reply.get_status()));
    }
}

void UsartDriver ::dmaReplyRxIsr(const Samd21::Dma::Reply& reply) {
    Fw::Success status;
    switch (reply.get_status()) {
        case Dma::Status::OK:
            // Make sure remaining bytes are consistent with our storage
            FW_ASSERT(reply.get_remainingBytes() <= USART_RX_BUFFER_SIZE, reply.get_remainingBytes());
            // Signal the high-RX watchdog that a full buffer just completed. schedIn
            // compares this across ticks to decide whether RX is busy (and thus
            // whether the suspend-inducing partial read is safe to run).
            this->m_rx_activity++;
            {
                // Carry the absolute count of bytes filled in the current buffer (high-water mark)
                // rather than a delta against m_active_processed. m_active_processed is only mutated
                // in activeIn_handler, so reading it here (in an ISR) would race; the absolute count
                // is computed purely from DMA state and the delta is resolved at consume time.
                CriticalSection cs;
                status = this->m_queue.enqueue(
                    Signal(SignalKind::RX_BUFFER_DONE,
                           static_cast<U16>(USART_RX_BUFFER_SIZE - static_cast<U16>(reply.get_remainingBytes()))));
            }
            // TODO(tumbar) If we are Rx-ing too fast, this will assert
            //              Maybe a better thing to do is to drop this buffer?
            FW_ASSERT(status == Fw::Success::SUCCESS, status);
            break;
        case Dma::Status::BUS_ERROR:
            // We tried to receive to a buffer on an invalid address
            FW_ASSERT(false, this->m_sercom);
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(reply.get_status()));
    }
}

}  // namespace Samd21

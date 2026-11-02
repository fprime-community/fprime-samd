// ======================================================================
// \title  SpiDriver.cpp
// \author tumbar
// \brief  cpp file for SpiDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/SpiDriver/SpiDriver.hpp"
#include <cstdint>

#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/FwIndexTypeAliasAc.h"
#include "fprime-samd/Drv/SpiDriver/SpiDriverHardware.hpp"
#include "fprime-samd/Drv/SpiDriver/SpiDriver_DmaChannelEnumAc.hpp"
#include "fprime-samd/Drv/Types/CriticalSection.hpp"
#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
#include "fprime-samd/Drv/Types/TransactionTypeEnumAc.hpp"

namespace Samd21 {

//! See SpiDriver.hpp for the derivation.
U8 SpiDriver ::calculateBaud(U32 f_ref_hz, U32 baud_rate_khz) {
    // Bound before scaling: the host cannot clock SCK faster than fref/2 (BAUD = 0), and a
    // larger request would also overflow the kHz -> Hz multiplication below.
    const U32 max_baud_rate_khz = f_ref_hz / 2 / 1000;
    FW_ASSERT(baud_rate_khz != 0);
    FW_ASSERT(baud_rate_khz <= max_baud_rate_khz, baud_rate_khz, max_baud_rate_khz);
    const U32 f_sck = baud_rate_khz * 1000;

    // Round the divisor up so the real SCK never exceeds the requested rate.
    const U32 divisor = 2 * f_sck;
    const U32 rounded_up = (f_ref_hz + divisor - 1) / divisor;
    FW_ASSERT(rounded_up >= 1, rounded_up);
    const U32 baud = rounded_up - 1;
    FW_ASSERT(baud <= 255, baud);
    return static_cast<U8>(baud);
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SpiDriver ::SpiDriver(const char* const compName)
    : SpiDriverComponentBase(compName),
      m_sercom(SercomKind::SERCOM_0),
      m_portNum(0),
      m_read(),
      m_write(),
      m_configured(false),
      m_busy(0),
      m_hardware_chip_select(false),
      m_pending(),
      m_refused() {
    for (FwIndexType i = 0; i < NUM_SPIWRITEREAD_INPUT_PORTS; i++) {
        this->m_pending[i].status = Drv::SpiStatus::SPI_OK;
        this->m_pending[i].pending = false;
        this->m_refused[i].status = Drv::SpiStatus::SPI_BUSY_ERR;
        this->m_refused[i].pending = false;
    }
}

SpiDriver ::~SpiDriver() {}

void SpiDriver ::configure(SercomKind sercom,
                           U32 baud_rate_khz,
                           DataOrder data_order,
                           ClockPolarity clock_polarity,
                           ClockPhase clock_phase,
                           DataInPinout data_in_pinout,
                           DataOutPinout data_out_pinout,
                           RunInStandby run_in_standby,
                           HardwareChipSelect hardware_chipselect) {
    FW_ASSERT(!this->m_configured);
    this->m_sercom = sercom;

    // Software chip selects are GPIOs; deassert them all before the peripheral starts
    // driving SCK/MOSI so no device sees clocks while selected.
    switch (hardware_chipselect) {
        case SpiDriver::HardwareChipSelect::DISABLED:
            this->m_hardware_chip_select = false;
            break;
        case SpiDriver::HardwareChipSelect::ENABLED:
            this->m_hardware_chip_select = true;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(hardware_chipselect));
    }
    if (!this->m_hardware_chip_select) {
        for (FwIndexType i = 0; i < this->getNum_chipSelectGpioOut_OutputPorts(); i++) {
            if (this->isConnected_chipSelectGpioOut_OutputPort(i)) {
                // Init-time check of the integration: the chip-select pin must already be an
                // opened output (GpioDriver configured before this component).
                const Drv::GpioStatus status = this->chipSelectGpioOut_out(i, Fw::Logic::HIGH);
                FW_ASSERT(status == Drv::GpioStatus::OP_OK, static_cast<FwAssertArgType>(status.e), i);
            }
        }
    }

    // Register for the ERROR interrupt that configure() enables below
    SpiHardware::SpiHal::registerIsr(sercom, SpiDriver::s_spiDriverIsrHandler, this);
    SpiHardware::SpiHal::configure(sercom, baud_rate_khz, data_order, clock_polarity, clock_phase, data_in_pinout,
                                   data_out_pinout, run_in_standby, hardware_chipselect);
    this->m_configured = true;
}

Drv::SpiStatus SpiDriver ::setBaudRate(U32 baud_rate_khz) {
    if (!this->m_configured) {
        return Drv::SpiStatus::SPI_CONFIG_ERR;
    }
    {
        // m_busy is cleared by the DMAC ISR. Idle cannot become busy behind our back: only
        // SpiWriteRead_handler (this same main context) starts a transfer.
        Samd21::CriticalSection cs;
        if (this->m_busy != 0) {
            return Drv::SpiStatus::SPI_BUSY_ERR;
        }
    }
    SpiHardware::SpiHal::setBaudRate(this->m_sercom, baud_rate_khz);
    return Drv::SpiStatus::SPI_OK;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::SpiStatus SpiDriver ::setBaudRate_handler(FwIndexType portNum, U32 baud_rate_khz) {
    return this->setBaudRate(baud_rate_khz);
}

void SpiDriver ::SpiWriteRead_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    // Caller contract (not a hardware condition): both streams clock the same number of bytes
    FW_ASSERT(writeBuffer.getSize() == readBuffer.getSize(), writeBuffer.getSize(), readBuffer.getSize());
    FW_ASSERT(writeBuffer.getSize() <= 0xFFFF);  // DMA block transfer count is 16 bits
    FW_ASSERT(portNum >= 0 && portNum < NUM_SPIWRITEREAD_INPUT_PORTS, portNum);
    {
        // m_busy and the pending records are written by the ISR paths; decide and claim
        // the bus atomically.
        Samd21::CriticalSection cs;
        // A slot may carry one transfer outcome and one refusal. If this slot's transfer is
        // in flight, or its last outcome still waits for activeIn, the ISR owns the outcome
        // record, so the refusal is latched in the slot's refusal record and delivered by
        // activeIn like every other completion -- nothing is called back from inside the
        // request, and no ISR path can collide with it. A third request on the same slot
        // before activeIn has run finds both records occupied: that is a client protocol
        // error (caller context, not hardware traffic) and asserts.
        const bool slotInFlight = (this->m_busy != 0) && (this->m_portNum == portNum);
        if (slotInFlight || this->m_pending[portNum].pending) {
            PendingReply& refusal = this->m_refused[portNum];
            FW_ASSERT(!refusal.pending, portNum);
            refusal.write = Samd21::ThinBuffer(writeBuffer);
            refusal.read = Samd21::ThinBuffer(readBuffer);
            refusal.status = Drv::SpiStatus::SPI_BUSY_ERR;
            refusal.pending = true;
            return;
        }
        if (!this->m_configured) {
            this->latchReply(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_CONFIG_ERR);
            return;
        }
        if (this->m_busy != 0) {
            this->latchReply(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_BUSY_ERR);
            return;
        }
        this->m_portNum = portNum;
        this->m_busy = BusyBit::RX_BUSY | BusyBit::TX_BUSY;
        this->m_read = Samd21::ThinBuffer(readBuffer);
        this->m_write = Samd21::ThinBuffer(writeBuffer);
    }

    if (!this->m_hardware_chip_select) {
        if (this->isConnected_chipSelectGpioOut_OutputPort(this->m_portNum)) {
            const Drv::GpioStatus status = this->chipSelectGpioOut_out(this->m_portNum, Fw::Logic::LOW);
            if (status != Drv::GpioStatus::OP_OK) {
                // No device selected: do not clock the bus, fail the request instead of
                // reporting SPI_OK for a transfer that reached nothing.
                Samd21::CriticalSection cs;
                this->m_busy = 0;
                this->latchReply(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OTHER_ERR);
                return;
            }
        }
    }

    // Queue up both Rx and Tx jobs
    // We need to do Rx first so that the job does kick off
    this->dmaTransactionOut_out(SpiDriver_DmaChannel::MISO, SercomUtil::rxDmaTrigger(this->m_sercom),
                                Dma::TransactionType::BEAT, Samd21::Dma::Priority::PRIORITY_0,
                                SpiHardware::SpiHal::getDataRegisterAddress(this->m_sercom),
                                static_cast<U32>(reinterpret_cast<uintptr_t>(readBuffer.getData())),
                                readBuffer.getSize(), Samd21::Dma::BeatSize::BYTE, false, true,
                                Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::DESTINATION);

    // Queuing the Tx job will trigger the transaction
    this->dmaTransactionOut_out(
        SpiDriver_DmaChannel::MOSI, SercomUtil::txDmaTrigger(this->m_sercom), Dma::TransactionType::BEAT,
        Samd21::Dma::Priority::PRIORITY_0, static_cast<U32>(reinterpret_cast<uintptr_t>(writeBuffer.getData())),
        SpiHardware::SpiHal::getDataRegisterAddress(this->m_sercom), writeBuffer.getSize(), Samd21::Dma::BeatSize::BYTE,
        true, false, Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::SOURCE);
}

bool SpiDriver ::activeIn_handler(FwIndexType portNum, U32 context) {
    for (FwIndexType slot = 0; slot < NUM_SPIWRITEREAD_INPUT_PORTS; slot++) {
        // Deliver the refusal (if any) first: it carries no data and tells the client its
        // re-request was dropped before the older request's outcome lets it re-arm.
        this->deliver(this->m_refused[slot], slot);
        this->deliver(this->m_pending[slot], slot);
    }
    return false;
}

void SpiDriver ::deliver(PendingReply& record, FwIndexType slot) {
    PendingReply reply = {};
    {
        // The ISR paths write the record; copy and clear it with interrupts masked, then
        // call the client with them enabled (the callback may start the next transfer).
        Samd21::CriticalSection cs;
        if (!record.pending) {
            return;
        }
        reply = record;
        record.pending = false;
    }
    Fw::Buffer write = reply.write.getBuffer();
    Fw::Buffer read = reply.read.getBuffer();
    if (this->isConnected_SpiWriteReadComplete_OutputPort(slot)) {
        this->SpiWriteReadComplete_out(slot, write, read, reply.status);
    }
}

void SpiDriver ::dmaReplyIn_handler(FwIndexType portNum, const Samd21::Dma::Reply& reply) {
    // Two replies (Tx, Rx) arrive in either order; the transfer is done when both are in.
    // A reply with nothing in flight, or for an unknown channel, cannot be acted on: it is
    // dropped rather than asserted on (no assert in response to bus traffic).
    if (this->m_busy == 0) {
        return;
    }
    U8 doneBit;
    SpiDriver_DmaChannel::T otherChannel;
    U8 otherBit;
    if (portNum == SpiDriver_DmaChannel::MISO) {
        doneBit = BusyBit::RX_BUSY;
        otherChannel = SpiDriver_DmaChannel::MOSI;
        otherBit = BusyBit::TX_BUSY;
    } else if (portNum == SpiDriver_DmaChannel::MOSI) {
        doneBit = BusyBit::TX_BUSY;
        otherChannel = SpiDriver_DmaChannel::MISO;
        otherBit = BusyBit::RX_BUSY;
    } else {
        return;
    }
    if ((this->m_busy & doneBit) == 0) {
        // Duplicate completion of a channel already accounted for: ignore
        return;
    }
    this->m_busy &= static_cast<U8>(~doneBit);
    if (reply.get_status() != Samd21::Dma::Status::OK) {
        // The stream faulted part-way (AHB bus error on a caller buffer): the bytes were
        // not all moved, so the transfer cannot be reported SPI_OK, and the other stream
        // would otherwise wait forever for its trigger. Stop it, release the bus and
        // fail the request. The DMA driver raises no reply for an aborted channel.
        if (this->m_busy & otherBit) {
            this->dmaTransactionAbortOut_out(otherChannel);
        }
        // Characters already clocked in but not collected would be read by the next
        // transfer: drain the receiver and clear the flags, as after an overrun.
        SpiHardware::SpiHal::acknowledgeError(this->m_sercom);
        this->m_busy = 0;
        this->deassertChipSelect();
        this->latchReply(this->m_portNum, this->m_write.getBuffer(), this->m_read.getBuffer(),
                         Drv::SpiStatus::SPI_OTHER_ERR);
        return;
    }
    if (this->m_busy == 0) {
        this->deassertChipSelect();
        this->latchReply(this->m_portNum, this->m_write.getBuffer(), this->m_read.getBuffer(), Drv::SpiStatus::SPI_OK);
    }
}

// ----------------------------------------------------------------------
// SERCOM interrupt handling
// ----------------------------------------------------------------------

void SpiDriver ::s_spiDriverIsrHandler(Fw::PassiveComponentBase* spiDriverRaw, SercomKind sercom) {
    auto spiDriver = reinterpret_cast<SpiDriver*>(spiDriverRaw);
    FW_ASSERT(spiDriver != nullptr);
    FW_ASSERT(sercom == spiDriver->m_sercom, sercom, spiDriver->m_sercom);
    spiDriver->isrHandler();
}

void SpiDriver ::isrHandler() {
    FW_ASSERT(this->m_configured, this->m_sercom);
    const SpiHardware::SpiErrorStatus status = SpiHardware::SpiHal::readErrorStatus(this->m_sercom);
    if (!status.error) {
        // ERROR is the only interrupt enabled; anything else is spurious
        return;
    }
    // Stop the DMA streams that have not completed before touching the peripheral, so
    // no further characters are pushed while the receiver is drained. The DMA driver
    // raises no reply for an aborted channel and clears its pending flags, so no stale
    // dmaReplyIn follows.
    if (this->m_busy & BusyBit::RX_BUSY) {
        this->dmaTransactionAbortOut_out(SpiDriver_DmaChannel::MISO);
    }
    if (this->m_busy & BusyBit::TX_BUSY) {
        this->dmaTransactionAbortOut_out(SpiDriver_DmaChannel::MOSI);
    }
    // Let the shifter finish, drain the receiver and clear the flags so the interrupt
    // cannot re-fire and the next transfer starts on an empty receive buffer.
    SpiHardware::SpiHal::acknowledgeError(this->m_sercom);
    if (this->m_busy == 0) {
        // Nothing in flight to fail (the transfer finished before the flag was seen)
        return;
    }
    this->m_busy = 0;
    this->deassertChipSelect();
    this->latchReply(this->m_portNum, this->m_write.getBuffer(), this->m_read.getBuffer(),
                     Drv::SpiStatus::SPI_OTHER_ERR);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void SpiDriver ::deassertChipSelect() {
    if (!this->m_hardware_chip_select) {
        if (this->isConnected_chipSelectGpioOut_OutputPort(this->m_portNum)) {
            // Release is best effort and may run in ISR context: the pin was proven an
            // opened output by configure() and by the status check that started this
            // transfer, so the status cannot differ from OP_OK here, and an ISR must not assert.
            (void)this->chipSelectGpioOut_out(this->m_portNum, Fw::Logic::HIGH);
        }
    }
}

void SpiDriver ::latchReply(FwIndexType portNum,
                            const Fw::Buffer& writeBuffer,
                            const Fw::Buffer& readBuffer,
                            Drv::SpiStatus::T status) {
    FW_ASSERT(portNum >= 0 && portNum < NUM_SPIWRITEREAD_INPUT_PORTS, portNum);
    PendingReply& slot = this->m_pending[portNum];
    // Invariant kept by SpiWriteRead_handler (a request on a slot with an undelivered
    // outcome goes to the refusal record instead), so this never fires on hardware traffic.
    FW_ASSERT(!slot.pending, portNum, static_cast<FwAssertArgType>(slot.status));
    slot.write = Samd21::ThinBuffer(writeBuffer);
    slot.read = Samd21::ThinBuffer(readBuffer);
    slot.status = status;
    slot.pending = true;
}

}  // namespace Samd21

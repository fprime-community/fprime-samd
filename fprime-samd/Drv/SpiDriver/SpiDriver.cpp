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
      m_hardware_chip_select(false) {}

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
                this->chipSelectGpioOut_out(i, Fw::Logic::HIGH);
            }
        }
    }

    SpiHardware::SpiHal::configure(sercom, baud_rate_khz, data_order, clock_polarity, clock_phase, data_in_pinout,
                                   data_out_pinout, run_in_standby, hardware_chipselect);

    this->m_configured = true;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SpiDriver ::SpiWriteRead_handler(FwIndexType portNum, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    FW_ASSERT(this->m_configured, this->m_sercom);
    FW_ASSERT(writeBuffer.getSize() == readBuffer.getSize(), writeBuffer.getSize(), readBuffer.getSize());
    FW_ASSERT(writeBuffer.getSize() <= 0xFFFF);  // We don't actually have this much memory but I'll check it anyway...

    if (this->m_busy != 0) {
        if (this->isConnected_SpiReply_OutputPort(portNum)) {
            this->SpiReply_out(portNum, writeBuffer, readBuffer, Drv::SpiStatus::SPI_OTHER_ERR);
        }

        return;
    }

    this->m_portNum = portNum;
    this->m_busy = BusyBit::RX_BUSY | BusyBit::TX_BUSY;
    this->m_read = Samd21::ThinBuffer(readBuffer);
    this->m_write = Samd21::ThinBuffer(writeBuffer);

    if (!this->m_hardware_chip_select) {
        if (this->isConnected_chipSelectGpioOut_OutputPort(this->m_portNum)) {
            this->chipSelectGpioOut_out(this->m_portNum, Fw::Logic::LOW);
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

void SpiDriver ::dmaReplyIn_handler(FwIndexType portNum, const Samd21::Dma::Reply& reply) {
    // We will get two replies (Tx, Rx) but the order is not guarenteed since they should finish at the same time.
    // We should reply to the transaction after we got both of the replies back
    FW_ASSERT(this->m_configured, this->m_sercom, this->m_portNum);
    FW_ASSERT(this->m_busy != 0, this->m_sercom, this->m_portNum);

    if (portNum == SpiDriver_DmaChannel::MISO) {
        FW_ASSERT(this->m_busy & BusyBit::RX_BUSY, this->m_sercom, portNum);
        this->m_busy &= ~BusyBit::RX_BUSY;
    } else if (portNum == SpiDriver_DmaChannel::MOSI) {
        FW_ASSERT(this->m_busy & BusyBit::TX_BUSY, this->m_sercom, portNum);
        this->m_busy &= ~BusyBit::TX_BUSY;
    } else {
        // What is this port?
        FW_ASSERT(false, this->m_sercom, portNum);
    }

    // Reply if both bufferrs were received
    if (!this->m_busy) {
        auto read = this->m_read.getBuffer();
        auto write = this->m_write.getBuffer();

        if (!this->m_hardware_chip_select) {
            if (this->isConnected_chipSelectGpioOut_OutputPort(this->m_portNum)) {
                this->chipSelectGpioOut_out(this->m_portNum, Fw::Logic::HIGH);
            }
        }

        this->SpiReply_out(this->m_portNum, write, read, Drv::SpiStatus::SPI_OK);
    }
}

}  // namespace Samd21

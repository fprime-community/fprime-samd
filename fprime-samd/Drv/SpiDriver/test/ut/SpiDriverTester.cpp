// ======================================================================
// \title  SpiDriverTester.cpp
// \brief  cpp file for SpiDriver component test harness implementation class
// ======================================================================

#include "SpiDriverTester.hpp"

#include <cstdint>

#include "STest/Pick/Pick.hpp"
#include "fprime-samd/Drv/Types/Sercom.hpp"

namespace Samd21 {

// Out-of-class definitions: the gtest macros bind these constants by reference
const FwSizeType SpiDriverTester::MAX_HISTORY_SIZE;
const FwSizeType SpiDriverTester::MAX_TEST_SIZE;
const U32 SpiDriverTester::TEST_F_REF_HZ;
const U32 SpiDriverTester::TEST_DATA_REGISTER;

static const SercomKind::T TEST_SERCOM = SercomKind::SERCOM_1;
static const U32 TEST_BAUD_KHZ = 4000;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SpiDriverTester ::SpiDriverTester()
    : SpiDriverGTestBase("SpiDriverTester", SpiDriverTester::MAX_HISTORY_SIZE),
      component("SpiDriver"),
      m_hardwareChipSelect(false) {
    this->initComponents();
    this->connectPorts();
    SpiHardware::resetStubState();
    for (FwIndexType port = 0; port < Samd21::SpiPorts; port++) {
        for (FwSizeType i = 0; i < MAX_TEST_SIZE; i++) {
            this->m_writeData[port][i] = static_cast<U8>(0xA0 + port + i);
            this->m_readData[port][i] = 0;
        }
    }
    this->clearAll();
}

SpiDriverTester ::~SpiDriverTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SpiDriverTester ::testConfigureSoftwareChipSelect() {
    // REQUIREMENT(SPI-CFG-001): configure() programs the peripheral once with the requested settings
    // REQUIREMENT(SPI-CFG-002): software chip selects are deasserted (HIGH) before the peripheral is enabled
    this->clearAll();
    this->component.configure(TEST_SERCOM, TEST_BAUD_KHZ, SpiDriver::DataOrder::MSB, SpiDriver::ClockPolarity::IdleLow,
                              SpiDriver::ClockPhase::SampleOnLeadingSck, SpiDriver::DataInPinout::PAD3,
                              SpiDriver::DataOutPinout::MOSI_0_SCK_1_CS_2, SpiDriver::RunInStandby::DISABLED,
                              SpiDriver::HardwareChipSelect::DISABLED);

    const SpiHardware::StubState& stub = SpiHardware::getStubState();
    ASSERT_TRUE(stub.configured);
    ASSERT_EQ(stub.configure_count, 1U);
    ASSERT_EQ(stub.sercom.e, TEST_SERCOM);
    ASSERT_EQ(stub.baud_rate_khz, TEST_BAUD_KHZ);
    ASSERT_EQ(stub.data_order, SpiDriver::DataOrder::MSB);
    ASSERT_EQ(stub.clock_polarity, SpiDriver::ClockPolarity::IdleLow);
    ASSERT_EQ(stub.clock_phase, SpiDriver::ClockPhase::SampleOnLeadingSck);
    ASSERT_EQ(stub.data_in_pinout, SpiDriver::DataInPinout::PAD3);
    ASSERT_EQ(stub.data_out_pinout, SpiDriver::DataOutPinout::MOSI_0_SCK_1_CS_2);
    ASSERT_EQ(stub.run_in_standby, SpiDriver::RunInStandby::DISABLED);
    ASSERT_EQ(stub.hardware_chipselect, SpiDriver::HardwareChipSelect::DISABLED);

    // Every connected chip select is driven HIGH exactly once, in port order
    ASSERT_from_chipSelectGpioOut_SIZE(Samd21::SpiPorts);
    for (FwIndexType port = 0; port < Samd21::SpiPorts; port++) {
        this->assertChipSelect(static_cast<FwSizeType>(port), port, Fw::Logic::HIGH);
    }
    ASSERT_from_dmaTransactionOut_SIZE(0);
    ASSERT_from_SpiReply_SIZE(0);
}

void SpiDriverTester ::testConfigureHardwareChipSelect() {
    // REQUIREMENT(SPI-CFG-003): with hardware chip select the GPIO ports are never driven
    this->clearAll();
    this->component.configure(TEST_SERCOM, TEST_BAUD_KHZ, SpiDriver::DataOrder::LSB, SpiDriver::ClockPolarity::IdleHigh,
                              SpiDriver::ClockPhase::SampleOnRisingSck, SpiDriver::DataInPinout::PAD0,
                              SpiDriver::DataOutPinout::MOSI_2_SCK_3_CS_1, SpiDriver::RunInStandby::ENABLED,
                              SpiDriver::HardwareChipSelect::ENABLED);

    const SpiHardware::StubState& stub = SpiHardware::getStubState();
    ASSERT_EQ(stub.configure_count, 1U);
    ASSERT_EQ(stub.data_order, SpiDriver::DataOrder::LSB);
    ASSERT_EQ(stub.clock_polarity, SpiDriver::ClockPolarity::IdleHigh);
    ASSERT_EQ(stub.clock_phase, SpiDriver::ClockPhase::SampleOnRisingSck);
    ASSERT_EQ(stub.data_in_pinout, SpiDriver::DataInPinout::PAD0);
    ASSERT_EQ(stub.data_out_pinout, SpiDriver::DataOutPinout::MOSI_2_SCK_3_CS_1);
    ASSERT_EQ(stub.run_in_standby, SpiDriver::RunInStandby::ENABLED);
    ASSERT_EQ(stub.hardware_chipselect, SpiDriver::HardwareChipSelect::ENABLED);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
}

void SpiDriverTester ::testConfigureTwice() {
    // REQUIREMENT(SPI-CFG-004): reconfiguring a configured driver is a programming error
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    ASSERT_DEATH_IF_SUPPORTED(this->configure(SpiDriver::HardwareChipSelect::DISABLED), "");
}

void SpiDriverTester ::testRequestBeforeConfigure() {
    // REQUIREMENT(SPI-REQ-001): a request before configure() asserts rather than touching hardware
    this->clearAll();
    this->m_writeBuffer[0].set(this->m_writeData[0], 4);
    this->m_readBuffer[0].set(this->m_readData[0], 4);
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_SpiWriteRead(0, this->m_writeBuffer[0], this->m_readBuffer[0]), "");
}

void SpiDriverTester ::testTransaction() {
    // REQUIREMENT(SPI-XFER-001): a request asserts the port's chip select and queues MISO then MOSI DMA
    // REQUIREMENT(SPI-XFER-002): the reply is emitted on the requesting port once both DMA channels complete,
    //                            in either order, with chip select deasserted first
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    for (FwIndexType port = 0; port < Samd21::SpiPorts; port++) {
        this->startTransaction(port, 1);
        this->finishTransaction(port, true);
        this->startTransaction(port, MAX_TEST_SIZE);
        this->finishTransaction(port, false);
    }
}

void SpiDriverTester ::testBusy() {
    // REQUIREMENT(SPI-XFER-003): one transaction at a time; an overlapping request is answered
    //                            synchronously with SPI_OTHER_ERR and generates no DMA or chip select traffic
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->startTransaction(0, 8);

    // Same port and a different port are both rejected while the first is in flight
    for (FwIndexType port = 0; port < 2; port++) {
        this->clearAll();
        this->m_writeBuffer[port].set(this->m_writeData[port], 3);
        this->m_readBuffer[port].set(this->m_readData[port], 3);
        this->invoke_to_SpiWriteRead(port, this->m_writeBuffer[port], this->m_readBuffer[port]);
        this->assertReply(port, Drv::SpiStatus::SPI_OTHER_ERR);
        ASSERT_from_dmaTransactionOut_SIZE(0);
        ASSERT_from_chipSelectGpioOut_SIZE(0);
    }

    // Still busy after only one DMA channel completes
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->m_writeBuffer[1].set(this->m_writeData[1], 3);
    this->m_readBuffer[1].set(this->m_readData[1], 3);
    this->invoke_to_SpiWriteRead(1, this->m_writeBuffer[1], this->m_readBuffer[1]);
    this->assertReply(1, Drv::SpiStatus::SPI_OTHER_ERR);

    // The original transaction completes untouched, then the bus is free again
    this->clearAll();
    this->m_writeBuffer[0].set(this->m_writeData[0], 8);
    this->m_readBuffer[0].set(this->m_readData[0], 8);
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->assertChipSelect(0, 0, Fw::Logic::HIGH);
    this->assertReply(0, Drv::SpiStatus::SPI_OK);
    this->startTransaction(1, 3);
    this->finishTransaction(1, true);
}

void SpiDriverTester ::testHardwareChipSelectTransaction() {
    // REQUIREMENT(SPI-XFER-004): with hardware chip select the transaction never drives the GPIO ports
    this->configure(SpiDriver::HardwareChipSelect::ENABLED);
    this->startTransaction(2, 16);
    this->finishTransaction(2, false);
}

void SpiDriverTester ::testSizeMismatch() {
    // REQUIREMENT(SPI-REQ-002): full duplex requires equal write/read sizes
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->m_writeBuffer[0].set(this->m_writeData[0], 4);
    this->m_readBuffer[0].set(this->m_readData[0], 5);
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_SpiWriteRead(0, this->m_writeBuffer[0], this->m_readBuffer[0]), "");
}

void SpiDriverTester ::testUnexpectedDmaReply() {
    // REQUIREMENT(SPI-XFER-005): a DMA completion with nothing in flight is a protocol violation
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    ASSERT_DEATH_IF_SUPPORTED(this->completeDma(SpiDriver_DmaChannel::MISO), "");
    ASSERT_DEATH_IF_SUPPORTED(this->completeDma(SpiDriver_DmaChannel::MOSI), "");

    // A duplicate completion of the same channel within one transaction also asserts
    this->startTransaction(0, 2);
    this->completeDma(SpiDriver_DmaChannel::MISO);
    ASSERT_DEATH_IF_SUPPORTED(this->completeDma(SpiDriver_DmaChannel::MISO), "");
}

void SpiDriverTester ::testCalculateBaud() {
    // REQUIREMENT(SPI-CFG-005): BAUD = fref/(2*fsck) - 1, rounded so SCK never exceeds the request
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 24000), 0);  // fref/2: fastest achievable
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 12000), 1);
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 4000), 5);
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 1000), 23);
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 100), 239);
    // Inexact rates round the divisor up: 7 MHz -> BAUD 3 (6 MHz actual), never BAUD 2 (8 MHz)
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 7000), 3);
    // Slowest representable rate: 48e6 / (2 * 94e3) = 255.3 -> BAUD 255
    ASSERT_EQ(SpiDriver::calculateBaud(TEST_F_REF_HZ, 94), 255);

    // REQUIREMENT(SPI-CFG-006): out-of-range requests assert instead of overflowing or truncating
    ASSERT_DEATH_IF_SUPPORTED(SpiDriver::calculateBaud(TEST_F_REF_HZ, 0), "");
    ASSERT_DEATH_IF_SUPPORTED(SpiDriver::calculateBaud(TEST_F_REF_HZ, 24001), "");
    ASSERT_DEATH_IF_SUPPORTED(SpiDriver::calculateBaud(TEST_F_REF_HZ, 93), "");
    // Would overflow the kHz -> Hz multiplication without the bound check
    ASSERT_DEATH_IF_SUPPORTED(SpiDriver::calculateBaud(TEST_F_REF_HZ, 0xFFFFFFFF), "");
}

void SpiDriverTester ::testRandomTransactions() {
    // REQUIREMENT(SPI-XFER-001..003): randomized ports, sizes, completion orders and busy rejections
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    for (U32 i = 0; i < 500; i++) {
        const FwIndexType port = static_cast<FwIndexType>(STest::Pick::lowerUpper(0, Samd21::SpiPorts - 1));
        const FwSizeType size = static_cast<FwSizeType>(STest::Pick::lowerUpper(1, MAX_TEST_SIZE));
        this->startTransaction(port, size);
        if (STest::Pick::lowerUpper(0, 3) == 0) {
            const FwIndexType other = static_cast<FwIndexType>(STest::Pick::lowerUpper(0, Samd21::SpiPorts - 1));
            if (other != port) {
                this->clearAll();
                this->m_writeBuffer[other].set(this->m_writeData[other], 1);
                this->m_readBuffer[other].set(this->m_readData[other], 1);
                this->invoke_to_SpiWriteRead(other, this->m_writeBuffer[other], this->m_readBuffer[other]);
                this->assertReply(other, Drv::SpiStatus::SPI_OTHER_ERR);
                ASSERT_from_dmaTransactionOut_SIZE(0);
            }
        }
        this->finishTransaction(port, STest::Pick::lowerUpper(0, 1) == 0);
    }
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void SpiDriverTester ::from_SpiReply_handler(FwIndexType portNum,
                                             Fw::Buffer& writeBuffer,
                                             Fw::Buffer& readBuffer,
                                             const Drv::SpiStatus& status) {
    const FwSizeType index = this->fromPortHistory_SpiReply->size();
    this->pushFromPortEntry_SpiReply(writeBuffer, readBuffer, status);
    if (index < MAX_HISTORY_SIZE) {
        this->m_replyPorts[index] = portNum;
    }
}

Drv::GpioStatus SpiDriverTester ::from_chipSelectGpioOut_handler(FwIndexType portNum, const Fw::Logic& state) {
    const FwSizeType index = this->fromPortHistory_chipSelectGpioOut->size();
    this->pushFromPortEntry_chipSelectGpioOut(state);
    if (index < MAX_HISTORY_SIZE) {
        this->m_chipSelectPorts[index] = portNum;
    }
    return Drv::GpioStatus::OP_OK;
}

void SpiDriverTester ::from_dmaTransactionOut_handler(FwIndexType portNum,
                                                      const Samd21::Dma::TriggerSource& trigger,
                                                      const Samd21::Dma::TransactionType& action,
                                                      const Samd21::Dma::Priority& priority,
                                                      U32 sourceAddr,
                                                      U32 destAddr,
                                                      U16 beat_count,
                                                      const Samd21::Dma::BeatSize& beatSize,
                                                      bool incrementSource,
                                                      bool incrementDestination,
                                                      const Samd21::Dma::AddressIncrementStepSize& stepSize,
                                                      const Samd21::Dma::StepSelection& stepSelection) {
    const FwSizeType index = this->fromPortHistory_dmaTransactionOut->size();
    this->pushFromPortEntry_dmaTransactionOut(trigger, action, priority, sourceAddr, destAddr, beat_count, beatSize,
                                              incrementSource, incrementDestination, stepSize, stepSelection);
    if (index < MAX_HISTORY_SIZE) {
        this->m_dmaPorts[index] = portNum;
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void SpiDriverTester ::clearAll() {
    this->clearHistory();
    for (FwSizeType i = 0; i < MAX_HISTORY_SIZE; i++) {
        this->m_replyPorts[i] = -1;
        this->m_chipSelectPorts[i] = -1;
        this->m_dmaPorts[i] = -1;
    }
}

void SpiDriverTester ::configure(SpiDriver::HardwareChipSelect hardwareChipSelect) {
    SpiHardware::resetStubState();
    SpiHardware::getStubState().data_register_address = TEST_DATA_REGISTER;
    this->m_hardwareChipSelect = (hardwareChipSelect == SpiDriver::HardwareChipSelect::ENABLED);
    this->component.configure(TEST_SERCOM, TEST_BAUD_KHZ, SpiDriver::DataOrder::MSB, SpiDriver::ClockPolarity::IdleLow,
                              SpiDriver::ClockPhase::SampleOnLeadingSck, SpiDriver::DataInPinout::PAD3,
                              SpiDriver::DataOutPinout::MOSI_0_SCK_1_CS_2, SpiDriver::RunInStandby::DISABLED,
                              hardwareChipSelect);
    this->clearAll();
}

void SpiDriverTester ::startTransaction(FwIndexType port, FwSizeType size) {
    this->clearAll();
    this->m_writeBuffer[port].set(this->m_writeData[port], static_cast<Fw::Buffer::SizeType>(size));
    this->m_readBuffer[port].set(this->m_readData[port], static_cast<Fw::Buffer::SizeType>(size));
    this->invoke_to_SpiWriteRead(port, this->m_writeBuffer[port], this->m_readBuffer[port]);

    // Chip select asserted (software only)
    if (this->m_hardwareChipSelect) {
        ASSERT_from_chipSelectGpioOut_SIZE(0);
    } else {
        ASSERT_from_chipSelectGpioOut_SIZE(1);
        this->assertChipSelect(0, port, Fw::Logic::LOW);
    }

    // MISO queued first (so the receiver is armed), then MOSI (which starts the clock)
    ASSERT_from_dmaTransactionOut_SIZE(2);
    ASSERT_EQ(this->m_dmaPorts[0], SpiDriver_DmaChannel::MISO);
    ASSERT_from_dmaTransactionOut(
        0, SercomUtil::rxDmaTrigger(TEST_SERCOM), Samd21::Dma::TransactionType::BEAT, Samd21::Dma::Priority::PRIORITY_0,
        TEST_DATA_REGISTER, address(this->m_readData[port]), static_cast<U16>(size), Samd21::Dma::BeatSize::BYTE, false,
        true, Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::DESTINATION);
    ASSERT_EQ(this->m_dmaPorts[1], SpiDriver_DmaChannel::MOSI);
    ASSERT_from_dmaTransactionOut(1, SercomUtil::txDmaTrigger(TEST_SERCOM), Samd21::Dma::TransactionType::BEAT,
                                  Samd21::Dma::Priority::PRIORITY_0, address(this->m_writeData[port]),
                                  TEST_DATA_REGISTER, static_cast<U16>(size), Samd21::Dma::BeatSize::BYTE, true, false,
                                  Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::SOURCE);

    // No reply until the DMA completes
    ASSERT_from_SpiReply_SIZE(0);
}

void SpiDriverTester ::completeDma(SpiDriver_DmaChannel::T channel) {
    const Samd21::Dma::Reply reply(Samd21::Dma::Status::OK, 0);
    this->invoke_to_dmaReplyIn(static_cast<FwIndexType>(channel), reply);
}

void SpiDriverTester ::finishTransaction(FwIndexType port, bool misoFirst) {
    this->clearAll();
    this->completeDma(misoFirst ? SpiDriver_DmaChannel::MISO : SpiDriver_DmaChannel::MOSI);
    ASSERT_from_SpiReply_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);

    this->completeDma(misoFirst ? SpiDriver_DmaChannel::MOSI : SpiDriver_DmaChannel::MISO);
    if (this->m_hardwareChipSelect) {
        ASSERT_from_chipSelectGpioOut_SIZE(0);
    } else {
        ASSERT_from_chipSelectGpioOut_SIZE(1);
        this->assertChipSelect(0, port, Fw::Logic::HIGH);
    }
    this->assertReply(port, Drv::SpiStatus::SPI_OK);
    ASSERT_from_dmaTransactionOut_SIZE(0);
}

void SpiDriverTester ::assertChipSelect(FwSizeType index, FwIndexType port, Fw::Logic::T state) {
    ASSERT_EQ(this->m_chipSelectPorts[index], port);
    ASSERT_from_chipSelectGpioOut(index, Fw::Logic(state));
}

void SpiDriverTester ::assertReply(FwIndexType port, Drv::SpiStatus::T status) {
    ASSERT_from_SpiReply_SIZE(1);
    ASSERT_EQ(this->m_replyPorts[0], port);
    const FromPortEntry_SpiReply& entry = this->fromPortHistory_SpiReply->at(0);
    ASSERT_EQ(entry.status.e, status);
    // The caller's buffers come back untouched (same memory, same size)
    ASSERT_EQ(entry.writeBuffer.getData(), this->m_writeBuffer[port].getData());
    ASSERT_EQ(entry.writeBuffer.getSize(), this->m_writeBuffer[port].getSize());
    ASSERT_EQ(entry.readBuffer.getData(), this->m_readBuffer[port].getData());
    ASSERT_EQ(entry.readBuffer.getSize(), this->m_readBuffer[port].getSize());
}

U32 SpiDriverTester ::address(const U8* data) {
    return static_cast<U32>(reinterpret_cast<uintptr_t>(data));
}

}  // namespace Samd21

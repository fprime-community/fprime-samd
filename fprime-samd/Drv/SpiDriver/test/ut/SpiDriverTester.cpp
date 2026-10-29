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
      m_hardwareChipSelect(false),
      m_chipSelectStatus(Drv::GpioStatus::OP_OK) {
    this->initComponents();
    this->connectPorts();
    SpiHardware::resetStubState();
    for (FwIndexType port = 0; port < Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS; port++) {
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
    ASSERT_from_chipSelectGpioOut_SIZE(Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS);
    for (FwIndexType port = 0; port < Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS; port++) {
        this->assertChipSelect(static_cast<FwSizeType>(port), port, Fw::Logic::HIGH);
    }
    ASSERT_from_dmaTransactionOut_SIZE(0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
}

void SpiDriverTester ::testConfigureHardwareChipSelect() {
    // REQUIREMENT(SPI-CFG-003): with hardware chip select the GPIO ports are never driven
    this->clearAll();
    this->component.configure(TEST_SERCOM, TEST_BAUD_KHZ, SpiDriver::DataOrder::LSB, SpiDriver::ClockPolarity::IdleHigh,
                              SpiDriver::ClockPhase::SampleOnTrailingSck, SpiDriver::DataInPinout::PAD0,
                              SpiDriver::DataOutPinout::MOSI_2_SCK_3_CS_1, SpiDriver::RunInStandby::ENABLED,
                              SpiDriver::HardwareChipSelect::ENABLED);

    const SpiHardware::StubState& stub = SpiHardware::getStubState();
    ASSERT_EQ(stub.configure_count, 1U);
    ASSERT_EQ(stub.data_order, SpiDriver::DataOrder::LSB);
    ASSERT_EQ(stub.clock_polarity, SpiDriver::ClockPolarity::IdleHigh);
    ASSERT_EQ(stub.clock_phase, SpiDriver::ClockPhase::SampleOnTrailingSck);
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
    // REQUIREMENT(SPI-REQ-001): a request before configure() touches no hardware and is refused
    //                            with SPI_CONFIG_ERR through the completion port, from activeIn
    this->clearAll();
    this->m_writeBuffer[0].set(this->m_writeData[0], 4);
    this->m_readBuffer[0].set(this->m_readData[0], 4);
    this->invoke_to_SpiWriteRead(0, this->m_writeBuffer[0], this->m_readBuffer[0]);
    ASSERT_from_dmaTransactionOut_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
    this->assertReply(0, Drv::SpiStatus::SPI_CONFIG_ERR);
}

void SpiDriverTester ::testTransaction() {
    // REQUIREMENT(SPI-XFER-001): a request asserts the port's chip select and queues MISO then MOSI DMA
    // REQUIREMENT(SPI-XFER-002): the reply is emitted on the requesting port once both DMA channels complete,
    //                            in either order, with chip select deasserted first
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    for (FwIndexType port = 0; port < Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS; port++) {
        this->startTransaction(port, 1);
        this->finishTransaction(port, true);
        this->startTransaction(port, MAX_TEST_SIZE);
        this->finishTransaction(port, false);
    }
}

void SpiDriverTester ::testBusy() {
    // REQUIREMENT(SPI-XFER-003): one transaction at a time; an overlapping request on another slot is
    //                            refused with SPI_BUSY_ERR (delivered from activeIn, never re-entrantly)
    //                            and generates no DMA or chip select traffic; the first transfer is unaffected
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->startTransaction(0, 8);
    this->clearAll();
    this->m_writeBuffer[1].set(this->m_writeData[1], 2);
    this->m_readBuffer[1].set(this->m_readData[1], 2);
    this->invoke_to_SpiWriteRead(1, this->m_writeBuffer[1], this->m_readBuffer[1]);
    ASSERT_from_dmaTransactionOut_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
    this->assertReply(1, Drv::SpiStatus::SPI_BUSY_ERR);

    // Another request on slot 1 is latched again; a request while that refusal is still
    // undelivered is latched in the slot's refusal record (SPI_BUSY_ERR), nothing is called
    // back from the request, and a third request asserts (client protocol error)
    this->clearAll();
    this->invoke_to_SpiWriteRead(1, this->m_writeBuffer[1], this->m_readBuffer[1]);
    this->invoke_to_SpiWriteRead(1, this->m_writeBuffer[1], this->m_readBuffer[1]);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_dmaTransactionOut_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_SpiWriteRead(1, this->m_writeBuffer[1], this->m_readBuffer[1]), "");
    // activeIn delivers both refusals on slot 1
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(2);
    this->assertDelivered(0, 1, Drv::SpiStatus::SPI_BUSY_ERR);
    this->assertDelivered(1, 1, Drv::SpiStatus::SPI_BUSY_ERR);
    this->clearAll();

    // The original transfer completes normally afterwards
    this->finishTransaction(0, true);
    this->startTransaction(1, 3);
    this->finishTransaction(1, true);
}

void SpiDriverTester ::testSameSlotReRequest() {
    // REQUIREMENT(SPI-XFER-003): a re-request on the slot whose transfer is in flight is refused with
    //                            SPI_BUSY_ERR from activeIn (ahead of the outcome, never from the request
    //                            call), generates no DMA or chip select traffic, never asserts, and the
    //                            in-flight transfer still completes; a third request asserts
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->startTransaction(0, 8);
    this->clearAll();
    this->invoke_to_SpiWriteRead(0, this->m_writeBuffer[0], this->m_readBuffer[0]);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_dmaTransactionOut_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
    // Both DMA completions arrive before activeIn ran: the OK outcome latches without asserting
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->assertChipSelect(0, 0, Fw::Logic::HIGH);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    // A third request on that slot before activeIn runs is a client protocol error
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_SpiWriteRead(0, this->m_writeBuffer[0], this->m_readBuffer[0]), "");
    this->clearAll();
    // activeIn delivers the refusal first, then the completion
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(2);
    this->assertDelivered(0, 0, Drv::SpiStatus::SPI_BUSY_ERR);
    this->assertDelivered(1, 0, Drv::SpiStatus::SPI_OK);
    this->clearAll();
    // A request on a slot whose completion is latched but undelivered is refused the same way
    this->startTransaction(0, 4);
    this->clearAll();
    this->finishTransaction(0, true);
    this->clearAll();
    this->startTransaction(0, 2);
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->clearAll();
    this->invoke_to_SpiWriteRead(0, this->m_writeBuffer[0], this->m_readBuffer[0]);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(2);
    this->assertDelivered(0, 0, Drv::SpiStatus::SPI_BUSY_ERR);
    this->assertDelivered(1, 0, Drv::SpiStatus::SPI_OK);
    this->clearAll();
    // And the slot is usable again
    this->startTransaction(0, 3);
    this->finishTransaction(0, false);
}

void SpiDriverTester ::testChipSelectFailure() {
    // REQUIREMENT(SPI-XFER-006, SPI-CFG-007): a chip select the GPIO driver cannot drive fails the request with
    //                          SPI_OTHER_ERR from activeIn and clocks nothing; configure() asserts on it
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->m_chipSelectStatus = Drv::GpioStatus::NOT_OPENED;
    this->clearAll();
    this->invoke_to_SpiWriteRead(1, this->m_writeBuffer[1], this->m_readBuffer[1]);
    ASSERT_from_chipSelectGpioOut_SIZE(1);
    this->assertChipSelect(0, 1, Fw::Logic::LOW);
    ASSERT_from_dmaTransactionOut_SIZE(0);
    this->assertReply(1, Drv::SpiStatus::SPI_OTHER_ERR);
    this->clearAll();
    // The bus was released: a request with a working chip select proceeds normally
    this->m_chipSelectStatus = Drv::GpioStatus::OP_OK;
    this->startTransaction(1, 4);
    this->finishTransaction(1, true);
    // configure() with an undriveable chip select is an integration error and asserts
    SpiDriverTester other;
    other.m_chipSelectStatus = Drv::GpioStatus::INVALID_MODE;
    ASSERT_DEATH_IF_SUPPORTED(other.configure(SpiDriver::HardwareChipSelect::DISABLED), "");
}

void SpiDriverTester ::testDmaBusError() {
    // REQUIREMENT(SPI-ERR-005): BUS_ERROR on either stream -> abort the other, drain/acknowledge the
    //                           receiver, CS high, SPI_OTHER_ERR, and the driver accepts a new
    //                           transaction afterwards
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    const SpiDriver_DmaChannel::T channels[] = {SpiDriver_DmaChannel::MOSI, SpiDriver_DmaChannel::MISO};
    for (FwIndexType i = 0; i < 2; i++) {
        const SpiDriver_DmaChannel::T failing = channels[i];
        const SpiDriver_DmaChannel::T other = channels[1 - i];
        this->startTransaction(1, 8);
        this->clearAll();
        const Samd21::Dma::Reply reply(Samd21::Dma::Status::BUS_ERROR, 5);
        this->invoke_to_dmaReplyIn(static_cast<FwIndexType>(failing), reply);
        ASSERT_from_dmaTransactionAbortOut_SIZE(1);
        this->assertAbort(0, other);
        ASSERT_EQ(SpiHardware::getStubState().acknowledge_error_count, static_cast<U32>(i + 1));
        ASSERT_from_chipSelectGpioOut_SIZE(1);
        this->assertChipSelect(0, 1, Fw::Logic::HIGH);
        this->assertReply(1, Drv::SpiStatus::SPI_OTHER_ERR);
        this->clearAll();
        // No reply follows for the aborted stream; a late one would be ignored
        this->completeDma(other);
        this->invoke_to_activeIn(0, 0);
        ASSERT_from_SpiWriteReadComplete_SIZE(0);
        ASSERT_from_chipSelectGpioOut_SIZE(0);
        this->clearAll();
        // Bus is free again
        this->startTransaction(0, 4);
        this->finishTransaction(0, true);
        this->clearAll();
    }
    // BUS_ERROR on the second completion (other stream already done) fails the transfer too
    this->startTransaction(2, 6);
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MISO);
    const Samd21::Dma::Reply reply(Samd21::Dma::Status::BUS_ERROR, 1);
    this->invoke_to_dmaReplyIn(static_cast<FwIndexType>(SpiDriver_DmaChannel::MOSI), reply);
    ASSERT_from_dmaTransactionAbortOut_SIZE(0);
    ASSERT_EQ(SpiHardware::getStubState().acknowledge_error_count, 3u);
    this->assertChipSelect(0, 2, Fw::Logic::HIGH);
    this->assertReply(2, Drv::SpiStatus::SPI_OTHER_ERR);
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
    // REQUIREMENT(SPI-XFER-005): a DMA completion with nothing in flight, or a duplicate completion of
    //                            one channel, is ignored: no assert, no reply, no chip select change
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);

    this->startTransaction(0, 2);
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->assertChipSelect(0, 0, Fw::Logic::HIGH);
    this->assertReply(0, Drv::SpiStatus::SPI_OK);
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
        const FwIndexType port =
            static_cast<FwIndexType>(STest::Pick::lowerUpper(0, Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS - 1));
        const FwSizeType size = static_cast<FwSizeType>(STest::Pick::lowerUpper(1, MAX_TEST_SIZE));
        this->startTransaction(port, size);
        if (STest::Pick::lowerUpper(0, 3) == 0) {
            const FwIndexType other = static_cast<FwIndexType>(
                STest::Pick::lowerUpper(0, Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS - 1));
            if (other != port) {
                this->clearAll();
                this->m_writeBuffer[other].set(this->m_writeData[other], 1);
                this->m_readBuffer[other].set(this->m_readData[other], 1);
                this->invoke_to_SpiWriteRead(other, this->m_writeBuffer[other], this->m_readBuffer[other]);
                this->assertReply(other, Drv::SpiStatus::SPI_BUSY_ERR);
                ASSERT_from_dmaTransactionOut_SIZE(0);
            }
        }
        this->finishTransaction(port, STest::Pick::lowerUpper(0, 1) == 0);
    }
}

void SpiDriverTester ::testReceiveOverrun() {
    // REQUIREMENT(SPI-ERR-001): the ISR is registered for the configured SERCOM
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    const SpiHardware::StubState& stub = SpiHardware::getStubState();
    ASSERT_NE(stub.isr_callback, nullptr);
    ASSERT_EQ(stub.isr_sercom, SercomKind(TEST_SERCOM));

    // REQUIREMENT(SPI-ERR-002): an overrun with both streams in flight aborts MISO and MOSI,
    //                           acknowledges the hardware, releases the chip select and fails the
    //                           transaction on the requesting port with the caller's buffers
    for (FwIndexType port = 0; port < Samd21::SpiDriver::NUM_SPIWRITEREAD_INPUT_PORTS; port++) {
        this->startTransaction(port, 16);
        this->clearAll();
        this->injectOverrun();
        ASSERT_EQ(stub.acknowledge_error_count, static_cast<U32>(port + 1));
        ASSERT_from_dmaTransactionAbortOut_SIZE(2);
        this->assertAbort(0, SpiDriver_DmaChannel::MISO);
        this->assertAbort(1, SpiDriver_DmaChannel::MOSI);
        ASSERT_from_chipSelectGpioOut_SIZE(1);
        this->assertChipSelect(0, port, Fw::Logic::HIGH);
        this->assertReply(port, Drv::SpiStatus::SPI_OTHER_ERR);
        ASSERT_from_dmaTransactionOut_SIZE(0);

        // REQUIREMENT(SPI-ERR-003): the bus is free again immediately after the failure
        this->startTransaction(port, 4);
        this->finishTransaction(port, port % 2 == 0);
    }

    // Hardware chip select: no GPIO traffic on the error path either
    SpiDriverTester hw;
    hw.configure(SpiDriver::HardwareChipSelect::ENABLED);
    hw.startTransaction(1, 8);
    hw.clearAll();
    hw.injectOverrun();
    ASSERT_EQ(hw.fromPortHistorySize_dmaTransactionAbortOut, 2u);
    hw.assertReply(1, Drv::SpiStatus::SPI_OTHER_ERR);
    ASSERT_EQ(hw.fromPortHistory_chipSelectGpioOut->size(), 0u);
}

void SpiDriverTester ::testReceiveOverrunAfterTxComplete() {
    // REQUIREMENT(SPI-ERR-002): only the stream still outstanding is aborted. MOSI finishing
    //                           first is the hardware case: every character was shifted out, the
    //                           receiver dropped one, and MISO can never reach its count.
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->startTransaction(0, 132);
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->clearAll();
    this->injectOverrun();
    ASSERT_from_dmaTransactionAbortOut_SIZE(1);
    this->assertAbort(0, SpiDriver_DmaChannel::MISO);
    this->assertChipSelect(0, 0, Fw::Logic::HIGH);
    this->assertReply(0, Drv::SpiStatus::SPI_OTHER_ERR);

    // A late MISO completion after the abort (the DMA driver normally clears it) is ignored
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);

    // Mirror case: MISO done, MOSI outstanding
    this->startTransaction(1, 8);
    this->completeDma(SpiDriver_DmaChannel::MISO);
    this->clearAll();
    this->injectOverrun();
    ASSERT_from_dmaTransactionAbortOut_SIZE(1);
    this->assertAbort(0, SpiDriver_DmaChannel::MOSI);
    this->assertReply(1, Drv::SpiStatus::SPI_OTHER_ERR);
    this->startTransaction(1, 2);
    this->finishTransaction(1, true);
}

void SpiDriverTester ::testSpuriousInterrupt() {
    // REQUIREMENT(SPI-ERR-004): an interrupt without INTFLAG.ERROR is ignored: no acknowledge,
    //                           no abort, no reply, transaction continues
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->startTransaction(0, 8);
    this->clearAll();
    SpiHardware::fireIsr();
    ASSERT_EQ(SpiHardware::getStubState().acknowledge_error_count, 0u);
    ASSERT_from_dmaTransactionAbortOut_SIZE(0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
    this->finishTransaction(0, true);
}

void SpiDriverTester ::testReceiveOverrunIdle() {
    // REQUIREMENT(SPI-ERR-004): an overrun with nothing in flight is acknowledged and otherwise
    //                           ignored; the next transaction proceeds normally
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->injectOverrun();
    ASSERT_EQ(SpiHardware::getStubState().acknowledge_error_count, 1u);
    ASSERT_from_dmaTransactionAbortOut_SIZE(0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
    this->startTransaction(2, 8);
    this->finishTransaction(2, false);

    // Before configure() there is no ISR registered, so the hardware hook is inert
    SpiDriverTester fresh;
    SpiHardware::resetStubState();
    SpiHardware::fireIsr();
    ASSERT_EQ(fresh.fromPortHistory_SpiWriteReadComplete->size(), 0u);
}

void SpiDriverTester ::testSetBaudRate() {
    // REQUIREMENT(SPI-CFG-008): the C++ method reports SPI_CONFIG_ERR before configure() and SPI_BUSY_ERR
    //                            while a transfer is in flight (rate unchanged), SPI_OK when idle
    ASSERT_EQ(this->component.setBaudRate(1000), Drv::SpiStatus::SPI_CONFIG_ERR);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 0u);
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    ASSERT_EQ(this->component.setBaudRate(400), Drv::SpiStatus::SPI_OK);
    ASSERT_EQ(SpiHardware::getStubState().baud_rate_khz, 400u);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 1u);
    this->startTransaction(0, 4);
    ASSERT_EQ(this->component.setBaudRate(8000), Drv::SpiStatus::SPI_BUSY_ERR);
    ASSERT_EQ(SpiHardware::getStubState().baud_rate_khz, 400u);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 1u);
    this->finishTransaction(0, true);
    ASSERT_EQ(this->component.setBaudRate(8000), Drv::SpiStatus::SPI_OK);
    ASSERT_EQ(SpiHardware::getStubState().baud_rate_khz, 8000u);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 2u);
}

void SpiDriverTester ::testSetBaudRatePort() {
    // REQUIREMENT(SPI-CFG-008): the setBaudRate port returns SPI_CONFIG_ERR before configure(), SPI_BUSY_ERR
    //                            while a transfer is in flight (rate unchanged) and SPI_OK otherwise
    // REQUIREMENT(SPI-CFG-009): after a baud change the SERCOM is enabled again and a full transfer
    //                            completes with SPI_OK (the bus is not left disabled)
    ASSERT_EQ(this->invoke_to_setBaudRate(0, 400), Drv::SpiStatus::SPI_CONFIG_ERR);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 0u);
    ASSERT_FALSE(SpiHardware::getStubState().enabled);

    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    ASSERT_TRUE(SpiHardware::getStubState().enabled);
    ASSERT_EQ(this->invoke_to_setBaudRate(0, 400), Drv::SpiStatus::SPI_OK);
    ASSERT_EQ(SpiHardware::getStubState().baud_rate_khz, 400u);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 1u);
    ASSERT_EQ(SpiHardware::getStubState().enable_count, 1u);
    ASSERT_TRUE(SpiHardware::getStubState().enabled);

    // In flight: refused, nothing touched
    this->startTransaction(0, 4);
    ASSERT_EQ(this->invoke_to_setBaudRate(0, 8000), Drv::SpiStatus::SPI_BUSY_ERR);
    ASSERT_EQ(SpiHardware::getStubState().baud_rate_khz, 400u);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 1u);

    // Both DMA streams done but activeIn not yet run: the latched reply does not block the change
    this->clearAll();
    this->completeDma(SpiDriver_DmaChannel::MOSI);
    this->completeDma(SpiDriver_DmaChannel::MISO);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_EQ(this->invoke_to_setBaudRate(0, 8000), Drv::SpiStatus::SPI_OK);
    ASSERT_EQ(SpiHardware::getStubState().baud_rate_khz, 8000u);
    ASSERT_EQ(SpiHardware::getStubState().set_baud_count, 2u);
    ASSERT_EQ(SpiHardware::getStubState().enable_count, 2u);
    ASSERT_TRUE(SpiHardware::getStubState().enabled);
    this->assertReply(0, Drv::SpiStatus::SPI_OK);

    // Not bricked: a complete transfer after the change is clocked out and completes with SPI_OK
    this->startTransaction(0, 8);
    ASSERT_EQ(SpiHardware::getStubState().data_register_reads_while_disabled, 0u);
    this->finishTransaction(0, false);
    ASSERT_TRUE(SpiHardware::getStubState().enabled);
    ASSERT_EQ(SpiHardware::getStubState().data_register_reads_while_disabled, 0u);
}

void SpiDriverTester ::testSchedIdle() {
    // REQUIREMENT(SPI-XFER-006): activeIn with nothing latched emits nothing, before and after configure()
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    this->configure(SpiDriver::HardwareChipSelect::DISABLED);
    this->invoke_to_activeIn(0, 0);
    this->invoke_to_activeIn(0, 1);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    ASSERT_from_chipSelectGpioOut_SIZE(0);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void SpiDriverTester ::from_SpiWriteReadComplete_handler(FwIndexType portNum,
                                                         Fw::Buffer& writeBuffer,
                                                         Fw::Buffer& readBuffer,
                                                         const Drv::SpiStatus& status) {
    const FwSizeType index = this->fromPortHistory_SpiWriteReadComplete->size();
    this->pushFromPortEntry_SpiWriteReadComplete(writeBuffer, readBuffer, status);
    if (index < MAX_HISTORY_SIZE) {
        this->m_replyPorts[index] = portNum;
    }
}

void SpiDriverTester ::from_dmaTransactionAbortOut_handler(FwIndexType portNum) {
    const FwSizeType index = this->fromPortHistorySize_dmaTransactionAbortOut;
    this->pushFromPortEntry_dmaTransactionAbortOut();
    if (index < MAX_HISTORY_SIZE) {
        this->m_abortPorts[index] = portNum;
    }
}

Drv::GpioStatus SpiDriverTester ::from_chipSelectGpioOut_handler(FwIndexType portNum, const Fw::Logic& state) {
    const FwSizeType index = this->fromPortHistory_chipSelectGpioOut->size();
    this->pushFromPortEntry_chipSelectGpioOut(state);
    if (index < MAX_HISTORY_SIZE) {
        this->m_chipSelectPorts[index] = portNum;
    }
    return this->m_chipSelectStatus;
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
        this->m_abortPorts[i] = -1;
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
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
}

void SpiDriverTester ::completeDma(SpiDriver_DmaChannel::T channel) {
    const Samd21::Dma::Reply reply(Samd21::Dma::Status::OK, 0);
    this->invoke_to_dmaReplyIn(static_cast<FwIndexType>(channel), reply);
}

void SpiDriverTester ::finishTransaction(FwIndexType port, bool misoFirst) {
    this->clearAll();
    this->completeDma(misoFirst ? SpiDriver_DmaChannel::MISO : SpiDriver_DmaChannel::MOSI);
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
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

void SpiDriverTester ::assertDelivered(FwSizeType index, FwIndexType port, Drv::SpiStatus::T status) {
    ASSERT_EQ(this->m_replyPorts[index], port);
    const FromPortEntry_SpiWriteReadComplete& entry = this->fromPortHistory_SpiWriteReadComplete->at(index);
    ASSERT_EQ(entry.status.e, status);
    ASSERT_EQ(entry.writeBuffer.getData(), this->m_writeBuffer[port].getData());
    ASSERT_EQ(entry.writeBuffer.getSize(), this->m_writeBuffer[port].getSize());
    ASSERT_EQ(entry.readBuffer.getData(), this->m_readBuffer[port].getData());
    ASSERT_EQ(entry.readBuffer.getSize(), this->m_readBuffer[port].getSize());
}

void SpiDriverTester ::assertReply(FwIndexType port, Drv::SpiStatus::T status) {
    // Replies are latched (ISR or refusal) and delivered only by activeIn, in main context
    ASSERT_from_SpiWriteReadComplete_SIZE(0);
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_SpiWriteReadComplete_SIZE(1);
    ASSERT_EQ(this->m_replyPorts[0], port);
    const FromPortEntry_SpiWriteReadComplete& entry = this->fromPortHistory_SpiWriteReadComplete->at(0);
    ASSERT_EQ(entry.status.e, status);
    // The caller's buffers come back untouched (same memory, same size)
    ASSERT_EQ(entry.writeBuffer.getData(), this->m_writeBuffer[port].getData());
    ASSERT_EQ(entry.writeBuffer.getSize(), this->m_writeBuffer[port].getSize());
    ASSERT_EQ(entry.readBuffer.getData(), this->m_readBuffer[port].getData());
    ASSERT_EQ(entry.readBuffer.getSize(), this->m_readBuffer[port].getSize());
}

void SpiDriverTester ::injectOverrun() {
    SpiHardware::StubState& stub = SpiHardware::getStubState();
    stub.error_status.error = true;
    stub.error_status.bufferOverflow = true;
    SpiHardware::fireIsr();
}

void SpiDriverTester ::assertAbort(FwSizeType index, SpiDriver_DmaChannel::T channel) {
    ASSERT_EQ(this->m_abortPorts[index], static_cast<FwIndexType>(channel));
}

U32 SpiDriverTester ::address(const U8* data) {
    return static_cast<U32>(reinterpret_cast<uintptr_t>(data));
}

}  // namespace Samd21

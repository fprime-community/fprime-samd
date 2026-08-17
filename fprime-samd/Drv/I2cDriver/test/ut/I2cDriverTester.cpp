// ======================================================================
// \title  I2cDriverTester.cpp
// \author tumbar
// \brief  cpp file for I2cDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/test/ut/I2cDriverTester.hpp"
#include "fprime-samd/Drv/Types/StatusEnumAc.hpp"
#include "samd-config/I2cDriverConfig.hpp"

namespace Samd21 {

// Out-of-line definition for ODR-used static constexpr member
constexpr U32 I2cDriverTester::DATA_REGISTER_ADDRESS;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

I2cDriverTester::I2cDriverTester()
    : I2cDriverGTestBase("I2cDriverTester", I2cDriverTester::MAX_HISTORY_SIZE), component("I2cDriver") {
    this->initComponents();
    this->connectPorts();
    I2cHardware::resetStubState();
    this->stub().data_register_address = DATA_REGISTER_ADDRESS;
    for (U32 i = 0; i < sizeof(m_write_data); i++) {
        m_write_data[i] = static_cast<U8>(i);
        m_read_data[i] = 0;
    }
}

I2cDriverTester::~I2cDriverTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

I2cHardware::StubState& I2cDriverTester::stub() {
    return I2cHardware::getStubState();
}

void I2cDriverTester::resetTest() {
    this->clearHistory();
}

void I2cDriverTester::configureStandard() {
    this->component.configure(SercomKind::SERCOM_0, I2cDriver::SclLowTimeout::ENABLED,
                              I2cDriver::InactiveTimeout::TIMEOUT_205_US, I2cDriver::ClockStretchMode::AFTER_ACK,
                              I2cDriver::Frequency::STANDARD_100KHZ, I2cDriver::ClientSclLowTimeout::ENABLED,
                              I2cDriver::HostSclLowTimeout::ENABLED, I2cDriver::SdaHold::HOLD_450_NS,
                              I2cDriver::PinUsage::TWO_WIRE, I2cDriver::RunInStandby::DISABLED);
}

void I2cDriverTester::injectDmaReply(I2cDriver_DmaChannel channel, Samd21::Dma::Status::T status, U32 remainingBytes) {
    Samd21::Dma::Reply reply(status, remainingBytes);
    this->invoke_to_dmaReplyIn(channel, reply);
}

void I2cDriverTester::fireIsr() {
    I2cHardware::fireIsr();
}

U32 I2cDriverTester::bufferAddr(const U8* data) {
    return static_cast<U32>(reinterpret_cast<uintptr_t>(data));
}

// ----------------------------------------------------------------------
// configure()
// ----------------------------------------------------------------------

void I2cDriverTester::testConfigureNominal() {
    this->resetTest();

    this->configureStandard();

    // Hardware was configured exactly once with the requested parameters
    ASSERT_TRUE(this->stub().configured);
    ASSERT_EQ(this->stub().configure_count, 1U);
    ASSERT_EQ(this->stub().sercom, SercomKind::SERCOM_0);
    ASSERT_EQ(this->stub().frequency, I2cDriver::Frequency::STANDARD_100KHZ);
    ASSERT_EQ(this->stub().pin_usage, I2cDriver::PinUsage::TWO_WIRE);
}

void I2cDriverTester::testConfigureAllParameters() {
    this->resetTest();

    // A non-default combination exercises the parameter plumbing through the HAL
    this->component.configure(SercomKind::SERCOM_3, I2cDriver::SclLowTimeout::DISABLED,
                              I2cDriver::InactiveTimeout::DISABLED, I2cDriver::ClockStretchMode::ALWAYS,
                              I2cDriver::Frequency::FAST_PLUS_1MHZ, I2cDriver::ClientSclLowTimeout::DISABLED,
                              I2cDriver::HostSclLowTimeout::DISABLED, I2cDriver::SdaHold::DISABLED,
                              I2cDriver::PinUsage::FOUR_WIRE, I2cDriver::RunInStandby::ENABLED);

    ASSERT_EQ(this->stub().sercom, SercomKind::SERCOM_3);
    ASSERT_EQ(this->stub().scl_low_timeout, I2cDriver::SclLowTimeout::DISABLED);
    ASSERT_EQ(this->stub().inactive_timeout, I2cDriver::InactiveTimeout::DISABLED);
    ASSERT_EQ(this->stub().clock_stretch_mode, I2cDriver::ClockStretchMode::ALWAYS);
    ASSERT_EQ(this->stub().frequency, I2cDriver::Frequency::FAST_PLUS_1MHZ);
    ASSERT_EQ(this->stub().client_scl_low_timeout, I2cDriver::ClientSclLowTimeout::DISABLED);
    ASSERT_EQ(this->stub().host_scl_low_timeout, I2cDriver::HostSclLowTimeout::DISABLED);
    ASSERT_EQ(this->stub().sda_hold, I2cDriver::SdaHold::DISABLED);
    ASSERT_EQ(this->stub().pin_usage, I2cDriver::PinUsage::FOUR_WIRE);
    ASSERT_EQ(this->stub().run_in_standby, I2cDriver::RunInStandby::ENABLED);
}

void I2cDriverTester::testConfigureRegistersIsr() {
    this->resetTest();

    this->configureStandard();

    // configure() registers the component's ISR trampoline for its SERCOM so
    // the hardware can dispatch interrupts back into the driver.
    ASSERT_NE(this->stub().isr_callback, nullptr);
    ASSERT_EQ(this->stub().isr_data, static_cast<Fw::PassiveComponentBase*>(&this->component));
    ASSERT_EQ(this->stub().isr_sercom, SercomKind::SERCOM_0);
}

// ----------------------------------------------------------------------
// read()
// ----------------------------------------------------------------------

void I2cDriverTester::testReadNominal() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x42;
    Fw::Buffer buffer(this->m_read_data, 8);
    this->invoke_to_read(0, addr, buffer);

    // A single RX DMA transaction is queued: source is the (fixed) DATA register,
    // destination increments into the read buffer.
    ASSERT_from_dmaTransactionOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).trigger, Samd21::Dma::TriggerSource::SERCOM0_RX);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).sourceAddr, DATA_REGISTER_ADDRESS);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).destAddr, bufferAddr(this->m_read_data));
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).beat_count, 8U);
    ASSERT_FALSE(this->fromPortHistory_dmaTransactionOut->at(0).incrementSource);
    ASSERT_TRUE(this->fromPortHistory_dmaTransactionOut->at(0).incrementDestination);

    // The transaction was kicked off on the peripheral via the HAL
    ASSERT_EQ(this->stub().begin_read_count, 1U);
    ASSERT_EQ(this->stub().begin_read_addr, addr);
    ASSERT_EQ(this->stub().begin_read_len, 8U);
    ASSERT_EQ(this->stub().begin_write_count, 0U);

    // No completion yet
    ASSERT_from_readComplete_SIZE(0);
}

void I2cDriverTester::testReadCompletion() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_read_data, 8);
    this->invoke_to_read(0, 0x42, buffer);

    // DMA signals the read completed
    this->injectDmaReply(I2cDriver_DmaChannel::READ, Samd21::Dma::Status::OK, 0);

    ASSERT_from_readComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_readComplete->at(0).status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(this->fromPortHistory_readComplete->at(0).buffer.getData(), this->m_read_data);

    // Back to idle: a second read is accepted
    this->invoke_to_read(0, 0x42, buffer);
    ASSERT_EQ(this->stub().begin_read_count, 2U);
}

void I2cDriverTester::testReadBusy() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_read_data, 8);
    this->invoke_to_read(0, 0x42, buffer);
    ASSERT_EQ(this->stub().begin_read_count, 1U);

    // A second read while the first is in-flight is rejected with OTHER_ERR and
    // does not touch the hardware again.
    Fw::Buffer buffer2(this->m_read_data, 4);
    this->invoke_to_read(0, 0x43, buffer2);

    ASSERT_EQ(this->stub().begin_read_count, 1U);
    ASSERT_from_readComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_readComplete->at(0).status, Drv::I2cStatus::I2C_OTHER_ERR);
}

// ----------------------------------------------------------------------
// write()
// ----------------------------------------------------------------------

void I2cDriverTester::testWriteNominal() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x21;
    Fw::Buffer buffer(this->m_write_data, 16);
    this->invoke_to_write(0, addr, buffer);

    // A single TX DMA transaction is queued: source increments through the write
    // buffer, destination is the fixed DATA register.
    ASSERT_from_dmaTransactionOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).trigger, Samd21::Dma::TriggerSource::SERCOM0_TX);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).sourceAddr, bufferAddr(this->m_write_data));
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).destAddr, DATA_REGISTER_ADDRESS);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).beat_count, 16U);
    ASSERT_TRUE(this->fromPortHistory_dmaTransactionOut->at(0).incrementSource);
    ASSERT_FALSE(this->fromPortHistory_dmaTransactionOut->at(0).incrementDestination);

    // A plain write generates a STOP condition (LENEN mode)
    ASSERT_EQ(this->stub().begin_write_count, 1U);
    ASSERT_EQ(this->stub().begin_write_addr, addr);
    ASSERT_EQ(this->stub().begin_write_len, 16U);
    ASSERT_TRUE(this->stub().begin_write_stop);

    ASSERT_from_writeComplete_SIZE(0);
}

void I2cDriverTester::testWriteCompletion() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_write_data, 16);
    this->invoke_to_write(0, 0x21, buffer);

    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);

    ASSERT_from_writeComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeComplete->at(0).status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(this->fromPortHistory_writeComplete->at(0).buffer.getData(), this->m_write_data);

    // Back to idle
    this->invoke_to_write(0, 0x21, buffer);
    ASSERT_EQ(this->stub().begin_write_count, 2U);
}

void I2cDriverTester::testWriteBusy() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_write_data, 16);
    this->invoke_to_write(0, 0x21, buffer);
    ASSERT_EQ(this->stub().begin_write_count, 1U);

    // Second write while busy: rejected, hardware untouched
    Fw::Buffer buffer2(this->m_write_data, 4);
    this->invoke_to_write(0, 0x22, buffer2);

    ASSERT_EQ(this->stub().begin_write_count, 1U);
    ASSERT_from_writeComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeComplete->at(0).status, Drv::I2cStatus::I2C_OTHER_ERR);
}

// ----------------------------------------------------------------------
// writeRead()
// ----------------------------------------------------------------------

void I2cDriverTester::testWriteReadNominal() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x50;
    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, addr, writeBuffer, readBuffer);

    // The write phase queues BOTH the write DMA and the read DMA up front, then
    // kicks off the write with NO stop condition (clock held for repeated start).
    ASSERT_from_dmaTransactionOut_SIZE(2);
    // First queued transfer is the write (TX trigger)
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).trigger, Samd21::Dma::TriggerSource::SERCOM0_TX);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).sourceAddr, bufferAddr(this->m_write_data));
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).destAddr, DATA_REGISTER_ADDRESS);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(0).beat_count, 4U);
    // Second queued transfer is the read (RX trigger)
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(1).trigger, Samd21::Dma::TriggerSource::SERCOM0_RX);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(1).sourceAddr, DATA_REGISTER_ADDRESS);
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(1).destAddr, bufferAddr(this->m_read_data));
    ASSERT_EQ(this->fromPortHistory_dmaTransactionOut->at(1).beat_count, 8U);

    ASSERT_EQ(this->stub().begin_write_count, 1U);
    ASSERT_EQ(this->stub().begin_write_addr, addr);
    ASSERT_FALSE(this->stub().begin_write_stop);  // repeated-start: no stop
    ASSERT_EQ(this->stub().begin_read_count, 0U);
}

void I2cDriverTester::testWriteReadFullSequence() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x50;
    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, addr, writeBuffer, readBuffer);
    this->clearHistory();

    // 1) Write DMA completes -> driver arms the master-on-bus interrupt and waits
    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);
    ASSERT_EQ(this->stub().enable_mb_count, 1U);
    ASSERT_from_writeReadComplete_SIZE(0);

    // 2) Master-on-bus ISR fires: the driver disables the MB interrupt and kicks
    //    off the read phase (read DMA was already queued, so no new queue).
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = true;
    this->fireIsr();

    ASSERT_EQ(this->stub().disable_mb_count, 1U);
    ASSERT_EQ(this->stub().acknowledge_mb_count, 1U);
    ASSERT_EQ(this->stub().begin_read_count, 1U);
    ASSERT_EQ(this->stub().begin_read_addr, addr);  // read half targets the stashed address
    ASSERT_EQ(this->stub().begin_read_len, 8U);

    // 3) Read DMA completes -> writeRead completion fires OK
    this->injectDmaReply(I2cDriver_DmaChannel::READ, Samd21::Dma::Status::OK, 0);
    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).writeBuffer.getData(), this->m_write_data);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).readBuffer.getData(), this->m_read_data);
}

void I2cDriverTester::testWriteReadBusy() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer);
    this->clearHistory();

    // Second writeRead while busy: rejected with OTHER_ERR
    Fw::Buffer wb2(this->m_write_data, 2);
    Fw::Buffer rb2(this->m_read_data, 2);
    this->invoke_to_writeRead(0, 0x51, wb2, rb2);

    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_OTHER_ERR);
    ASSERT_from_dmaTransactionOut_SIZE(0);
}

void I2cDriverTester::testWriteReadPointerNack() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x50;
    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, addr, writeBuffer, readBuffer);

    // Write DMA completes -> driver arms the master-on-bus interrupt and waits.
    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);
    this->clearHistory();

    // The master-on-bus ISR fires, but the client NACKed the address/register
    // pointer (RXNACK set). The driver must NOT issue the repeated-start read:
    // instead it disables MB, aborts the pre-armed read DMA, and reports the
    // write-read as a WRITE_ERR.
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = true;
    this->stub().interrupt_status.rxNack = true;
    this->fireIsr();

    // MB acknowledged and disabled; no read was kicked off.
    ASSERT_EQ(this->stub().acknowledge_mb_count, 1U);
    ASSERT_EQ(this->stub().disable_mb_count, 1U);
    ASSERT_EQ(this->stub().begin_read_count, 0U);

    // The pre-armed read DMA was aborted and the caller got WRITE_ERR.
    ASSERT_from_dmaTransactionAbortOut_SIZE(1);  // READ only
    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_WRITE_ERR);

    // Driver returned to IDLE: a fresh transaction is accepted.
    this->invoke_to_writeRead(0, addr, writeBuffer, readBuffer);
    ASSERT_EQ(this->stub().begin_write_count, 2U);
}

void I2cDriverTester::testWriteReadMbAlreadyLatched() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x50;
    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, addr, writeBuffer, readBuffer);
    this->clearHistory();

    // MB is a LEVEL condition: the write's address/data byte may already be ACKed
    // (INTFLAG.MB set) by the time the write-DMA completion ISR runs. Model that by
    // arming masterOnBus BEFORE injecting the write-DMA reply. If the driver only
    // enabled the MB interrupt and waited for a fresh edge, the read half would
    // never be kicked off and the transaction would wedge.
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = true;

    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);

    // The MB interrupt was enabled (armed) then, seeing MB already latched, the
    // handoff ran inline: MB acknowledged, MB interrupt disabled, read kicked off.
    ASSERT_EQ(this->stub().enable_mb_count, 1U);
    ASSERT_EQ(this->stub().acknowledge_mb_count, 1U);
    ASSERT_EQ(this->stub().disable_mb_count, 1U);
    ASSERT_EQ(this->stub().begin_read_count, 1U);
    ASSERT_EQ(this->stub().begin_read_addr, addr);  // read half targets the stashed address
    ASSERT_EQ(this->stub().begin_read_len, 8U);

    // No separate MB ISR was needed; the read DMA completes the transaction OK.
    this->injectDmaReply(I2cDriver_DmaChannel::READ, Samd21::Dma::Status::OK, 0);
    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_OK);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).writeBuffer.getData(), this->m_write_data);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).readBuffer.getData(), this->m_read_data);
}

void I2cDriverTester::testWriteReadMbAlreadyLatchedSpuriousIsr() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    const U32 addr = 0x50;
    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, addr, writeBuffer, readBuffer);
    this->clearHistory();

    // Service the handoff inline (MB already latched at write-DMA completion). This
    // advances the driver to the read phase (m_state == WRITE_READ_READING).
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = true;
    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);
    ASSERT_EQ(this->stub().begin_read_count, 1U);

    // On real hardware, enabling INTENSET.MB while MB was already latched asserts the
    // SERCOM line and latches the NVIC pending bit BEFORE the inline path acks MB and
    // disables the interrupt. That stale pending bit still fires one SERCOM interrupt
    // afterwards, now with neither ERROR nor MB set (the inline ack cleared MB). The
    // handler must treat this as a benign spurious wake and NOT assert -- this is the
    // regression that tripped FW_ASSERT(status.masterOnBus) with m_state==WRITE_READ_READING.
    this->clearHistory();
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = false;
    this->fireIsr();

    // Spurious wake is a no-op: nothing acknowledged, nothing disabled, no event, and
    // the in-flight read is untouched.
    ASSERT_EQ(this->stub().acknowledge_mb_count, 1U);  // still just the inline ack
    ASSERT_EQ(this->stub().disable_mb_count, 1U);
    ASSERT_EQ(this->stub().begin_read_count, 1U);
    ASSERT_EVENTS_UnexpectedInterrupt_SIZE(0);
    ASSERT_from_writeReadComplete_SIZE(0);

    // The read DMA still completes the transaction cleanly.
    this->injectDmaReply(I2cDriver_DmaChannel::READ, Samd21::Dma::Status::OK, 0);
    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_OK);
}

// ----------------------------------------------------------------------
// ISR error handling
// ----------------------------------------------------------------------

void I2cDriverTester::testIsrErrorIdleUnexpected() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // An error interrupt while IDLE: the specific error is logged, plus an
    // UnexpectedInterrupt warning since no transaction was in flight.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.busError = true;
    this->fireIsr();

    ASSERT_EVENTS_I2cBusError_SIZE(1);
    ASSERT_EVENTS_I2cBusError(0, SercomKind::SERCOM_0, I2cDriver_I2cError::BUS_ERROR);
    ASSERT_EVENTS_UnexpectedInterrupt_SIZE(1);
    ASSERT_EVENTS_UnexpectedInterrupt(0, SercomKind::SERCOM_0, I2cDriver_I2CInterrupt::BUS_ERROR);

    // The error flags were acknowledged
    ASSERT_EQ(this->stub().acknowledge_errors_count, 1U);
}

void I2cDriverTester::testIsrErrorDuringRead() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_read_data, 8);
    this->invoke_to_read(0, 0x42, buffer);
    this->clearHistory();

    // An arbitration-lost error during a read: abort the read DMA and reply
    // READ_ERR to the caller.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.arbLost = true;
    this->fireIsr();

    ASSERT_EVENTS_I2cBusError_SIZE(1);
    ASSERT_EVENTS_I2cBusError(0, SercomKind::SERCOM_0, I2cDriver_I2cError::ARBITRATION_LOST);

    ASSERT_from_dmaTransactionAbortOut_SIZE(1);
    ASSERT_from_readComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_readComplete->at(0).status, Drv::I2cStatus::I2C_READ_ERR);

    // Driver returned to IDLE: a fresh read is accepted
    this->invoke_to_read(0, 0x42, buffer);
    ASSERT_EQ(this->stub().begin_read_count, 2U);
}

void I2cDriverTester::testIsrErrorDuringWrite() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_write_data, 16);
    this->invoke_to_write(0, 0x21, buffer);
    this->clearHistory();

    // A bus error during a write: abort the write DMA and reply WRITE_ERR.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.busError = true;
    this->fireIsr();

    ASSERT_from_dmaTransactionAbortOut_SIZE(1);
    ASSERT_from_writeComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeComplete->at(0).status, Drv::I2cStatus::I2C_WRITE_ERR);
}

void I2cDriverTester::testIsrErrorDuringWriteReadWriting() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer);
    this->clearHistory();

    // An error while still writing the write-read: both DMA channels aborted,
    // writeRead completion fires WRITE_ERR.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.lowTimeout = true;
    this->fireIsr();

    ASSERT_EVENTS_I2cBusError(0, SercomKind::SERCOM_0, I2cDriver_I2cError::SCL_LOW_TIMEOUT);
    ASSERT_from_dmaTransactionAbortOut_SIZE(2);  // WRITE + READ
    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_WRITE_ERR);
}

void I2cDriverTester::testIsrErrorDuringWriteReadReading() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer writeBuffer(this->m_write_data, 4);
    Fw::Buffer readBuffer(this->m_read_data, 8);
    this->invoke_to_writeRead(0, 0x50, writeBuffer, readBuffer);

    // Advance to the READING sub-state: write DMA done, then MB interrupt.
    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = true;
    this->fireIsr();
    this->clearHistory();

    // Now an error during the read half: only the READ channel is aborted
    // (write already finished) and the completion reports READ_ERR.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.masterOnBus = false;
    this->stub().interrupt_status.slaveExtTimeout = true;
    this->fireIsr();

    ASSERT_EVENTS_I2cBusError(0, SercomKind::SERCOM_0, I2cDriver_I2cError::SLAVE_SCL_EXTEND_TIMEOUT);
    ASSERT_from_dmaTransactionAbortOut_SIZE(1);  // READ only
    ASSERT_from_writeReadComplete_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_writeReadComplete->at(0).status, Drv::I2cStatus::I2C_READ_ERR);
}

void I2cDriverTester::testIsrAllErrorFlags() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // All six error flags set at once (during IDLE) -> one event per flag, and
    // the error counter increments for each.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.busError = true;
    this->stub().interrupt_status.arbLost = true;
    this->stub().interrupt_status.lowTimeout = true;
    this->stub().interrupt_status.masterExtTimeout = true;
    this->stub().interrupt_status.slaveExtTimeout = true;
    this->stub().interrupt_status.lengthError = true;
    this->fireIsr();

    ASSERT_EVENTS_I2cBusError_SIZE(6);

    // Reported error count surfaces as telemetry on the next report tick
    this->invoke_to_reportTelemetryIn(0, 0);
    ASSERT_TLM_BusErrorCount_SIZE(1);
    ASSERT_TLM_BusErrorCount(0, 6U);
}

// ----------------------------------------------------------------------
// ISR master-on-bus handling
// ----------------------------------------------------------------------

void I2cDriverTester::testIsrMasterOnBusUnexpected() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // A master-on-bus interrupt while IDLE (not in the write-read wait state) is
    // acknowledged and flagged as unexpected.
    this->stub().interrupt_status.error = false;
    this->stub().interrupt_status.masterOnBus = true;
    this->fireIsr();

    ASSERT_EQ(this->stub().acknowledge_mb_count, 1U);
    ASSERT_EVENTS_UnexpectedInterrupt_SIZE(1);
    ASSERT_EVENTS_UnexpectedInterrupt(0, SercomKind::SERCOM_0, I2cDriver_I2CInterrupt::MASTER_ON_BUS);
}

// ----------------------------------------------------------------------
// dmaReplyIn channel validation
// ----------------------------------------------------------------------

void I2cDriverTester::testDmaReplyWrongChannelRead() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // In READ state, a reply on the WRITE channel is invalid.
    Fw::Buffer buffer(this->m_read_data, 8);
    this->invoke_to_read(0, 0x42, buffer);
    this->clearHistory();

    this->injectDmaReply(I2cDriver_DmaChannel::WRITE, Samd21::Dma::Status::OK, 0);

    ASSERT_EVENTS_InvalidDmaReply_SIZE(1);
    ASSERT_EVENTS_InvalidDmaReply(0, SercomKind::SERCOM_0, I2cDriver_DmaChannel::WRITE, I2cDriver_DmaChannel::READ);
    // Still in READ: no completion fired
    ASSERT_from_readComplete_SIZE(0);
}

void I2cDriverTester::testDmaReplyWrongChannelWrite() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // In WRITE state, a reply on the READ channel is invalid.
    Fw::Buffer buffer(this->m_write_data, 16);
    this->invoke_to_write(0, 0x21, buffer);
    this->clearHistory();

    this->injectDmaReply(I2cDriver_DmaChannel::READ, Samd21::Dma::Status::OK, 0);

    ASSERT_EVENTS_InvalidDmaReply_SIZE(1);
    ASSERT_EVENTS_InvalidDmaReply(0, SercomKind::SERCOM_0, I2cDriver_DmaChannel::READ, I2cDriver_DmaChannel::WRITE);
    ASSERT_from_writeComplete_SIZE(0);
}

void I2cDriverTester::testDmaReplyIdleIgnored() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // A DMA reply while IDLE is silently ignored (returns early).
    this->injectDmaReply(I2cDriver_DmaChannel::READ, Samd21::Dma::Status::OK, 0);

    ASSERT_from_readComplete_SIZE(0);
    ASSERT_from_writeComplete_SIZE(0);
    ASSERT_EVENTS_InvalidDmaReply_SIZE(0);
}

// ----------------------------------------------------------------------
// Telemetry
// ----------------------------------------------------------------------

void I2cDriverTester::testReportTelemetry() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Arm a representative bus status: OWNER, clock held, RX NACK.
    this->stub().bus_status.busState = 0x2;  // OWNER
    this->stub().bus_status.clockHold = true;
    this->stub().bus_status.rxNack = true;
    this->stub().bus_status.slaveOnBus = false;
    this->stub().bus_status.masterOnBus = false;

    this->invoke_to_reportTelemetryIn(0, 0);

    // The bus-error counter is always emitted, regardless of the telemetry gate.
    ASSERT_TLM_BusErrorCount_SIZE(1);

    if (Samd21::I2cDriverConfig::I2C_ENABLE_DEBUG_TELEMETRY) {
        ASSERT_TLM_BusState_SIZE(1);
        ASSERT_TLM_BusState(0, I2cDriver_I2cBusState::OWNER);
        ASSERT_TLM_ClockHold_SIZE(1);
        ASSERT_TLM_ClockHold(0, true);
        ASSERT_TLM_ReceiveNotAcknowledged_SIZE(1);
        ASSERT_TLM_ReceiveNotAcknowledged(0, true);
        ASSERT_TLM_DeviceOnBus(0, I2cDriver_DeviceOnBusFlag::NONE);
    } else {
        // Diagnostics suppressed: only the error counter goes out.
        ASSERT_TLM_BusState_SIZE(0);
        ASSERT_TLM_ClockHold_SIZE(0);
        ASSERT_TLM_ReceiveNotAcknowledged_SIZE(0);
        ASSERT_TLM_DeviceOnBus_SIZE(0);
    }
}

void I2cDriverTester::testReportTelemetryDeviceOnBus() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // This case only exercises the diagnostic device-on-bus decode, which is
    // compile-time gated; skip it when telemetry is disabled.
    if (!Samd21::I2cDriverConfig::I2C_ENABLE_DEBUG_TELEMETRY) {
        return;
    }

    // Both master and slave flagged on the bus.
    this->stub().bus_status.busState = 0x1;  // IDLE
    this->stub().bus_status.slaveOnBus = true;
    this->stub().bus_status.masterOnBus = true;

    this->invoke_to_reportTelemetryIn(0, 0);

    ASSERT_TLM_BusState(0, I2cDriver_I2cBusState::IDLE);
    ASSERT_TLM_DeviceOnBus_SIZE(1);
    ASSERT_TLM_DeviceOnBus(0, I2cDriver_DeviceOnBusFlag::MASTER_AND_SLAVE_ON_BUS);
}

// ----------------------------------------------------------------------
// Command
// ----------------------------------------------------------------------

void I2cDriverTester::testClearErrors() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Accumulate some errors via the ISR.
    this->stub().interrupt_status.error = true;
    this->stub().interrupt_status.busError = true;
    this->fireIsr();
    this->invoke_to_reportTelemetryIn(0, 0);
    ASSERT_TLM_BusErrorCount(0, 1U);
    this->clearHistory();

    // CLEAR_ERRORS resets the counter and responds OK.
    this->sendCmd_CLEAR_ERRORS(0, 10);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, I2cDriverComponentBase::OPCODE_CLEAR_ERRORS, 10, Fw::CmdResponse::OK);

    // Telemetry now reports zero errors.
    this->invoke_to_reportTelemetryIn(0, 0);
    ASSERT_TLM_BusErrorCount(0, 0U);
}

}  // namespace Samd21

// ======================================================================
// \title  UsartDriverTester.cpp
// \author tumbar
// \brief  cpp file for UsartDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/UsartDriver/test/ut/UsartDriverTester.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "STest/Pick/Pick.hpp"

namespace Samd21 {

// Out-of-line definitions for ODR-used static constexpr members
constexpr U32 UsartDriverTester::DATA_REGISTER_ADDRESS;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

UsartDriverTester::UsartDriverTester()
    : UsartDriverGTestBase("UsartDriverTester", UsartDriverTester::MAX_HISTORY_SIZE),
      component("UsartDriver"),
      m_rx_remaining(USART_RX_BUFFER_SIZE) {
    this->initComponents();
    this->connectPorts();
    UsartHardware::resetStubState();
    this->stub().data_register_address = DATA_REGISTER_ADDRESS;
    for (U32 i = 0; i < sizeof(m_tx_data); i++) {
        m_tx_data[i] = static_cast<U8>(i);
    }
}

UsartDriverTester::~UsartDriverTester() {}

// ----------------------------------------------------------------------
// Overridden output-port handlers
// ----------------------------------------------------------------------

Samd21::Dma::Writeback UsartDriverTester::from_dmaRxRead_handler(FwIndexType portNum) {
    // Record the call in history (the default base handler does this; overriding
    // it here means we must push the entry ourselves).
    this->pushFromPortEntry_dmaRxRead();
    // btctrl, btcnt, srcaddr, dstaddr, descaddr
    // The driver only reads btcnt (remaining beat count).
    return Samd21::Dma::Writeback(0, this->m_rx_remaining, 0, 0, 0);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

UsartHardware::StubState& UsartDriverTester::stub() {
    return UsartHardware::getStubState();
}

void UsartDriverTester::resetTest() {
    this->clearHistory();
}

void UsartDriverTester::configureStandard() {
    this->component.configure(SercomKind::SERCOM_0, UsartDriver::RxPinOut::PAD3, UsartDriver::TxPinOut::PAD0,
                              UsartDriver::ClockMode::INTERNAL, UsartDriver::CommunicationMode::ASYNC,
                              UsartDriver::BaudRate::BAUD_115200, UsartDriver::DataOrder::LSB_FIRST,
                              UsartDriver::DataBits::BITS_8, UsartDriver::StopBits::ONE, UsartDriver::Parity::NONE);

    // configure() queues both RX buffers to the RX DMA channel. The destination
    // address of each queued transfer is the (truncated) base pointer of RX
    // buffer A and B respectively -- capture them so RX tests can assert the
    // forwarded buffer pointers without reaching into private component state.
    ASSERT_GE(this->fromPortHistory_dmaQueueOut->size(), 2U);
    this->m_rx_buffer_addr[0] = this->fromPortHistory_dmaQueueOut->at(0).destAddr;
    this->m_rx_buffer_addr[1] = this->fromPortHistory_dmaQueueOut->at(1).destAddr;
}

U32 UsartDriverTester::rxBufferAddr(U32 index) const {
    return this->m_rx_buffer_addr[index];
}

U32 UsartDriverTester::recvBufferAddr(U32 historyIndex) const {
    const U8* data = this->fromPortHistory_recv->at(historyIndex).buffer.getData();
    return static_cast<U32>(reinterpret_cast<uintptr_t>(data));
}

void UsartDriverTester::setRxRemainingBytes(U16 remaining) {
    this->m_rx_remaining = remaining;
}

void UsartDriverTester::injectDmaReply(UsartDriver_DmaChannel channel,
                                       Samd21::Dma::Status::T status,
                                       U32 remainingBytes) {
    Samd21::Dma::Reply reply(status, remainingBytes);
    this->invoke_to_dmaReplyIn(channel, reply);
}

void UsartDriverTester::sendBuffer(Fw::Buffer& buffer) {
    this->invoke_to_send(0, buffer);
}

// ----------------------------------------------------------------------
// configure()
// ----------------------------------------------------------------------

void UsartDriverTester::testConfigureNominal() {
    // REQUIREMENT("SAMD21-UART-001: configure SERCOM for USART operation");
    this->resetTest();

    this->configureStandard();

    // Hardware was configured exactly once with the requested parameters
    ASSERT_TRUE(this->stub().configured);
    ASSERT_EQ(this->stub().configure_count, 1U);
    ASSERT_EQ(this->stub().sercom, SercomKind::SERCOM_0);
    ASSERT_EQ(this->stub().baud_rate, UsartDriver::BaudRate::BAUD_115200);
    ASSERT_EQ(this->stub().data_order, UsartDriver::DataOrder::LSB_FIRST);

    // ready_out() fires once configured
    ASSERT_from_ready_SIZE(1);
}

void UsartDriverTester::testConfigureAllParameters() {
    // REQUIREMENT("SAMD21-UART-001: configurable formats/parity/stop bits/order");
    this->resetTest();

    // Non-default combination exercises the parameter plumbing
    this->component.configure(SercomKind::SERCOM_3, UsartDriver::RxPinOut::PAD1, UsartDriver::TxPinOut::PAD2_XCK_PAD3,
                              UsartDriver::ClockMode::EXTERNAL, UsartDriver::CommunicationMode::SYNC,
                              UsartDriver::BaudRate::BAUD_921600, UsartDriver::DataOrder::MSB_FIRST,
                              UsartDriver::DataBits::BITS_9, UsartDriver::StopBits::TWO, UsartDriver::Parity::ODD);

    ASSERT_EQ(this->stub().sercom, SercomKind::SERCOM_3);
    ASSERT_EQ(this->stub().rx, UsartDriver::RxPinOut::PAD1);
    ASSERT_EQ(this->stub().tx, UsartDriver::TxPinOut::PAD2_XCK_PAD3);
    ASSERT_EQ(this->stub().clock, UsartDriver::ClockMode::EXTERNAL);
    ASSERT_EQ(this->stub().mode, UsartDriver::CommunicationMode::SYNC);
    ASSERT_EQ(this->stub().baud_rate, UsartDriver::BaudRate::BAUD_921600);
    ASSERT_EQ(this->stub().data_order, UsartDriver::DataOrder::MSB_FIRST);
    ASSERT_EQ(this->stub().data_bits, UsartDriver::DataBits::BITS_9);
    ASSERT_EQ(this->stub().stop_bits, UsartDriver::StopBits::TWO);
    ASSERT_EQ(this->stub().parity, UsartDriver::Parity::ODD);
}

void UsartDriverTester::testConfigureQueuesRxBuffers() {
    // REQUIREMENT("SAMD21-UART-003: double-buffering for continuous RX");
    this->resetTest();

    this->configureStandard();

    // Both RX buffers should be queued to the RX DMA channel, plus the
    // circular-mode configuration signal.
    ASSERT_from_dmaQueueOut_SIZE(2);
    ASSERT_from_dmaRxCircular_SIZE(1);

    // Both queued transfers target the RX DMA: source is the DATA register,
    // destination increments into the RX buffer.
    for (U32 i = 0; i < 2; i++) {
        ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(i).trigger, Samd21::Dma::TriggerSource::SERCOM0_RX);
        ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(i).sourceAddr, DATA_REGISTER_ADDRESS);
        ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(i).beat_count, USART_RX_BUFFER_SIZE);
        ASSERT_TRUE(this->fromPortHistory_dmaQueueOut->at(i).incrementDestination);
        ASSERT_FALSE(this->fromPortHistory_dmaQueueOut->at(i).incrementSource);
    }
}

// ----------------------------------------------------------------------
// send() / TX path
// ----------------------------------------------------------------------

void UsartDriverTester::testSendNominal() {
    // REQUIREMENT("SAMD21-UART-002/005: DMA-based queued TX");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_tx_data, 16);
    this->sendBuffer(buffer);

    // A single TX DMA transaction is submitted with the correct parameters
    ASSERT_from_dmaQueueOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(0).trigger, Samd21::Dma::TriggerSource::SERCOM0_TX);
    ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(0).destAddr, DATA_REGISTER_ADDRESS);
    ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(0).beat_count, 16U);
    ASSERT_TRUE(this->fromPortHistory_dmaQueueOut->at(0).incrementSource);
    ASSERT_FALSE(this->fromPortHistory_dmaQueueOut->at(0).incrementDestination);

    // No completion yet, so the buffer has not been returned
    ASSERT_from_sendReturnOut_SIZE(0);
}

void UsartDriverTester::testSendQueueFull() {
    // REQUIREMENT("SAMD21-UART-005: full queue returns SEND_RETRY");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Fill the TX queue (depth USART_TX_BUFFER_N) without completing any
    for (U32 i = 0; i < UsartDriverConfig::USART_TX_BUFFER_N; i++) {
        Fw::Buffer buffer(this->m_tx_data, 8);
        this->sendBuffer(buffer);
    }
    ASSERT_from_dmaQueueOut_SIZE(UsartDriverConfig::USART_TX_BUFFER_N);
    ASSERT_from_sendReturnOut_SIZE(0);

    // One more send must be rejected immediately with SEND_RETRY
    Fw::Buffer overflow(this->m_tx_data, 8);
    this->sendBuffer(overflow);

    ASSERT_from_dmaQueueOut_SIZE(UsartDriverConfig::USART_TX_BUFFER_N);  // no new DMA request
    ASSERT_from_sendReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_sendReturnOut->at(0).status, Drv::ByteStreamStatus::SEND_RETRY);
}

void UsartDriverTester::testSendCompletion() {
    // REQUIREMENT("SAMD21-UART-005: async completion returns buffer OP_OK");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_tx_data, 16);
    this->sendBuffer(buffer);

    // DMA signals completion in ISR context
    this->injectDmaReply(UsartDriver_DmaChannel::TX, Samd21::Dma::Status::OK, 0);

    // Signal is deferred; nothing returned until activeIn processes the queue
    ASSERT_from_sendReturnOut_SIZE(0);

    this->invoke_to_activeIn(0, 0);

    ASSERT_from_sendReturnOut_SIZE(1);
    ASSERT_EQ(this->fromPortHistory_sendReturnOut->at(0).status, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EQ(this->fromPortHistory_sendReturnOut->at(0).buffer.getData(), this->m_tx_data);
}

void UsartDriverTester::testSendChannelError() {
    // TX DMA bus errors are unrecoverable (invalid buffer pointer) and assert immediately
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Queue a TX buffer
    Fw::Buffer b0(this->m_tx_data, 8);
    this->sendBuffer(b0);

    // A DMA bus error indicates the driver handed the DMA an invalid buffer
    // pointer. This is an unrecoverable programming error, so the ISR asserts
    // immediately rather than deferring the failure to the signal queue.
    ASSERT_DEATH_IF_SUPPORTED(this->injectDmaReply(UsartDriver_DmaChannel::TX, Samd21::Dma::Status::BUS_ERROR, 8),
                              "Assert:");
}

// ----------------------------------------------------------------------
// sendSync()
// ----------------------------------------------------------------------

void UsartDriverTester::testSendSyncNominal() {
    // REQUIREMENT("SAMD21-UART-006: synchronous blocking transmission");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    U8 data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    Fw::Buffer buffer(data, sizeof(data));

    Drv::ByteStreamStatus status = this->invoke_to_sendSync(0, buffer);

    ASSERT_EQ(status, Drv::ByteStreamStatus::OP_OK);
    ASSERT_EQ(this->stub().send_sync_count, 1U);
    ASSERT_EQ(this->stub().send_sync_sercom, SercomKind::SERCOM_0);
    ASSERT_EQ(this->stub().send_sync_size, sizeof(data));
    for (U32 i = 0; i < sizeof(data); i++) {
        ASSERT_EQ(this->stub().send_sync_data[i], data[i]);
    }

    // Sync path uses no DMA
    ASSERT_from_dmaQueueOut_SIZE(0);
}

// ----------------------------------------------------------------------
// RX path
// ----------------------------------------------------------------------

void UsartDriverTester::testSchedInNoData() {
    // REQUIREMENT("SAMD21-UART-004: no new bytes -> no signal, no recv");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Full buffer remaining => zero bytes received
    this->setRxRemainingBytes(USART_RX_BUFFER_SIZE);
    this->invoke_to_schedIn(0, 0);

    // schedIn only reads the DMA writeback; without new data no signal enqueues
    ASSERT_from_dmaRxRead_SIZE(1);

    // No downstream data, no partial event
    this->invoke_to_activeIn(0, 0);
    ASSERT_from_recv_SIZE(0);
}

void UsartDriverTester::testSchedInPartial() {
    // REQUIREMENT("SAMD21-UART-004: partial RX frame extraction on tick");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // 10 bytes received into buffer A (remaining = SIZE - 10)
    const U16 received = 10;
    this->setRxRemainingBytes(USART_RX_BUFFER_SIZE - received);
    this->invoke_to_schedIn(0, 0);
    this->invoke_to_activeIn(0, 0);

    // The 10 fresh bytes are forwarded downstream from buffer A offset 0
    ASSERT_from_recv_SIZE(1);
    ASSERT_EQ(this->recvBufferAddr(0), this->rxBufferAddr(0));
    ASSERT_EQ(this->fromPortHistory_recv->at(0).buffer.getSize(), received);
    ASSERT_EQ(this->fromPortHistory_recv->at(0).status, Drv::ByteStreamStatus::OP_OK);
}

void UsartDriverTester::testRxMultiplePartials() {
    // REQUIREMENT("SAMD21-UART-004: repeated partials advance the offset");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // First tick: 10 bytes
    this->setRxRemainingBytes(USART_RX_BUFFER_SIZE - 10);
    this->invoke_to_schedIn(0, 0);
    this->invoke_to_activeIn(0, 0);

    // Second tick: total 25 bytes => 15 new bytes at offset 10
    this->setRxRemainingBytes(USART_RX_BUFFER_SIZE - 25);
    this->invoke_to_schedIn(0, 0);
    this->invoke_to_activeIn(0, 0);

    ASSERT_from_recv_SIZE(2);
    ASSERT_EQ(this->recvBufferAddr(1), this->rxBufferAddr(0) + 10);
    ASSERT_EQ(this->fromPortHistory_recv->at(1).buffer.getSize(), 15U);
}

void UsartDriverTester::testRxBufferDone() {
    // REQUIREMENT("SAMD21-UART-003: full buffer drains remainder and flips");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // RX DMA reports the buffer completely filled (0 remaining)
    this->injectDmaReply(UsartDriver_DmaChannel::RX, Samd21::Dma::Status::OK, 0);
    this->invoke_to_activeIn(0, 0);

    // Entire buffer A is forwarded downstream
    ASSERT_from_recv_SIZE(1);
    ASSERT_EQ(this->recvBufferAddr(0), this->rxBufferAddr(0));
    ASSERT_EQ(this->fromPortHistory_recv->at(0).buffer.getSize(), USART_RX_BUFFER_SIZE);
}

void UsartDriverTester::testRxBufferFlip() {
    // REQUIREMENT("SAMD21-UART-003: active buffer flips A->B->A on completion");
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Complete buffer A -> active flips to B
    this->injectDmaReply(UsartDriver_DmaChannel::RX, Samd21::Dma::Status::OK, 0);
    this->invoke_to_activeIn(0, 0);

    // A partial on buffer B should now index into m_rx[1]
    this->setRxRemainingBytes(USART_RX_BUFFER_SIZE - 4);
    this->invoke_to_schedIn(0, 0);  // service the schedIn twice to trigger the IDLE watchdog
    this->invoke_to_schedIn(0, 0);
    this->invoke_to_activeIn(0, 0);

    ASSERT_from_recv_SIZE(2);
    ASSERT_EQ(this->recvBufferAddr(1), this->rxBufferAddr(1));
    ASSERT_EQ(this->fromPortHistory_recv->at(1).buffer.getSize(), 4U);

    // Complete buffer B -> active flips back to A
    this->clearHistory();
    this->injectDmaReply(UsartDriver_DmaChannel::RX, Samd21::Dma::Status::OK, 0);
    this->invoke_to_activeIn(0, 0);

    // Remainder of B after the 4 already-processed bytes is forwarded
    ASSERT_from_recv_SIZE(1);
    ASSERT_EQ(this->recvBufferAddr(0), this->rxBufferAddr(1) + 4);
    ASSERT_EQ(this->fromPortHistory_recv->at(0).buffer.getSize(), USART_RX_BUFFER_SIZE - 4);
}

void UsartDriverTester::testRecvReturnIn() {
    // recvReturnIn is a no-op; buffers stay in the DMA chain
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    Fw::Buffer buffer(this->m_tx_data, sizeof(this->m_tx_data));
    this->invoke_to_recvReturnIn(0, buffer);

    // No observable side effects
    ASSERT_from_dmaQueueOut_SIZE(0);
    ASSERT_from_recv_SIZE(0);
}

// ----------------------------------------------------------------------
// Unconfigured behavior
// ----------------------------------------------------------------------

void UsartDriverTester::testSchedInUnconfigured() {
    // schedIn before configure() is a safe no-op
    this->resetTest();

    this->invoke_to_schedIn(0, 0);

    // Never touched the DMA
    ASSERT_from_dmaRxRead_SIZE(0);
    ASSERT_from_recv_SIZE(0);
}

void UsartDriverTester::testActiveInUnconfigured() {
    // activeIn before configure() is a safe no-op
    this->resetTest();

    this->invoke_to_activeIn(0, 0);

    ASSERT_from_sendReturnOut_SIZE(0);
    ASSERT_from_recv_SIZE(0);
}

// ----------------------------------------------------------------------
// Trigger-source mapping
// ----------------------------------------------------------------------

void UsartDriverTester::testTriggerSourceMapping() {
    // REQUIREMENT("SAMD21-UART-002: correct DMA trigger per SERCOM for TX and RX");
    // Configure a fresh component on each SERCOM and confirm both the RX trigger
    // (requested during configure()) and the TX trigger (requested during a
    // send()) match the SERCOM.
    const SercomKind::T sercoms[] = {SercomKind::SERCOM_0, SercomKind::SERCOM_1, SercomKind::SERCOM_2,
                                     SercomKind::SERCOM_3, SercomKind::SERCOM_4, SercomKind::SERCOM_5};
    const Samd21::Dma::TriggerSource::T expected_rx[] = {
        Samd21::Dma::TriggerSource::SERCOM0_RX, Samd21::Dma::TriggerSource::SERCOM1_RX,
        Samd21::Dma::TriggerSource::SERCOM2_RX, Samd21::Dma::TriggerSource::SERCOM3_RX,
        Samd21::Dma::TriggerSource::SERCOM4_RX, Samd21::Dma::TriggerSource::SERCOM5_RX};
    const Samd21::Dma::TriggerSource::T expected_tx[] = {
        Samd21::Dma::TriggerSource::SERCOM0_TX, Samd21::Dma::TriggerSource::SERCOM1_TX,
        Samd21::Dma::TriggerSource::SERCOM2_TX, Samd21::Dma::TriggerSource::SERCOM3_TX,
        Samd21::Dma::TriggerSource::SERCOM4_TX, Samd21::Dma::TriggerSource::SERCOM5_TX};

    for (U32 i = 0; i < 6; i++) {
        this->clearHistory();
        // Build a dedicated component + harness wiring for this SERCOM.
        UsartDriver comp("UsartDriverTrig");
        comp.init(0);
        // Wire the ports we need to observe (both RX and TX DMA channels share
        // the dmaQueueOut array port).
        comp.set_dmaQueueOut_OutputPort(UsartDriver_DmaChannel::RX,
                                        this->get_from_dmaQueueOut(UsartDriver_DmaChannel::RX));
        comp.set_dmaQueueOut_OutputPort(UsartDriver_DmaChannel::TX,
                                        this->get_from_dmaQueueOut(UsartDriver_DmaChannel::TX));
        comp.set_dmaRxCircular_OutputPort(0, this->get_from_dmaRxCircular(0));

        comp.configure(sercoms[i], UsartDriver::RxPinOut::PAD3, UsartDriver::TxPinOut::PAD0,
                       UsartDriver::ClockMode::INTERNAL, UsartDriver::CommunicationMode::ASYNC,
                       UsartDriver::BaudRate::BAUD_115200, UsartDriver::DataOrder::LSB_FIRST,
                       UsartDriver::DataBits::BITS_8, UsartDriver::StopBits::ONE, UsartDriver::Parity::NONE);

        // configure() queues both RX buffers with the SERCOM's RX trigger
        ASSERT_from_dmaQueueOut_SIZE(2);
        ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(0).trigger, expected_rx[i]);
        ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(1).trigger, expected_rx[i]);

        // A send() requests a TX DMA transfer with the SERCOM's TX trigger
        Fw::Buffer buffer(this->m_tx_data, 4);
        comp.get_send_InputPort(0)->invoke(buffer);
        ASSERT_from_dmaQueueOut_SIZE(3);
        ASSERT_EQ(this->fromPortHistory_dmaQueueOut->at(2).trigger, expected_tx[i]);
    }
}

void UsartDriverTester::testRxChannelError() {
    // RX DMA bus errors are unrecoverable (invalid buffer pointer) and assert immediately
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // A DMA bus error on the RX channel means the DMA was pointed at an invalid
    // buffer address -- an unrecoverable programming error. The ISR asserts
    // immediately rather than attempting to recover the RX channel.
    ASSERT_DEATH_IF_SUPPORTED(this->injectDmaReply(UsartDriver_DmaChannel::RX, Samd21::Dma::Status::BUS_ERROR, 4),
                              "Assert:");
}

}  // namespace Samd21

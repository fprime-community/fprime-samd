// ======================================================================
// \title  UsartDriverTestMain.cpp
// \author tumbar
// \brief  cpp file for UsartDriver component test main function
// ======================================================================

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/UsartDriver/test/ut/UsartDriverTester.hpp"

// ----------------------------------------------------------------------
// configure()
// ----------------------------------------------------------------------

TEST(Configure, Nominal) {
    COMMENT("configure() programs the SERCOM once and announces readiness");
    Samd21::UsartDriverTester tester;
    tester.testConfigureNominal();
}

TEST(Configure, AllParameters) {
    COMMENT("configure() forwards every format parameter to the HAL");
    Samd21::UsartDriverTester tester;
    tester.testConfigureAllParameters();
}

TEST(Configure, QueuesRxBuffers) {
    COMMENT("configure() primes both RX buffers into the RX DMA channel");
    Samd21::UsartDriverTester tester;
    tester.testConfigureQueuesRxBuffers();
}

// ----------------------------------------------------------------------
// send() / TX path
// ----------------------------------------------------------------------

TEST(Send, Nominal) {
    COMMENT("send() enqueues a TX DMA transaction with correct descriptors");
    Samd21::UsartDriverTester tester;
    tester.testSendNominal();
}

TEST(Send, QueueFull) {
    COMMENT("send() returns SEND_RETRY once the TX queue is full");
    Samd21::UsartDriverTester tester;
    tester.testSendQueueFull();
}

TEST(Send, Completion) {
    COMMENT("TX DMA completion returns the buffer OP_OK via activeIn");
    Samd21::UsartDriverTester tester;
    tester.testSendCompletion();
}

TEST(Send, ChannelError) {
    COMMENT("TX DMA bus error asserts (invalid buffer pointer handed to DMA)");
    Samd21::UsartDriverTester tester;
    tester.testSendChannelError();
}

// ----------------------------------------------------------------------
// sendSync()
// ----------------------------------------------------------------------

TEST(SendSync, Nominal) {
    COMMENT("sendSync() transmits synchronously through the HAL with no DMA");
    Samd21::UsartDriverTester tester;
    tester.testSendSyncNominal();
}

// ----------------------------------------------------------------------
// RX path
// ----------------------------------------------------------------------

TEST(Rx, SchedInNoData) {
    COMMENT("schedIn with no new bytes forwards nothing downstream");
    Samd21::UsartDriverTester tester;
    tester.testSchedInNoData();
}

TEST(Rx, SchedInPartial) {
    COMMENT("schedIn forwards newly-received bytes and emits RxPartial");
    Samd21::UsartDriverTester tester;
    tester.testSchedInPartial();
}

TEST(Rx, MultiplePartials) {
    COMMENT("successive partials advance the processed offset");
    Samd21::UsartDriverTester tester;
    tester.testRxMultiplePartials();
}

TEST(Rx, BufferDone) {
    COMMENT("a completed RX buffer forwards the full buffer and emits RxFull");
    Samd21::UsartDriverTester tester;
    tester.testRxBufferDone();
}

TEST(Rx, BufferFlip) {
    COMMENT("the active RX buffer flips A->B->A on completion");
    Samd21::UsartDriverTester tester;
    tester.testRxBufferFlip();
}

TEST(Rx, RecvReturnIn) {
    COMMENT("recvReturnIn is a no-op; buffers remain owned by the DMA");
    Samd21::UsartDriverTester tester;
    tester.testRecvReturnIn();
}

TEST(Rx, ChannelError) {
    COMMENT("RX DMA bus error asserts (invalid buffer pointer handed to DMA)");
    Samd21::UsartDriverTester tester;
    tester.testRxChannelError();
}

// ----------------------------------------------------------------------
// Unconfigured behavior
// ----------------------------------------------------------------------

TEST(Unconfigured, SchedIn) {
    COMMENT("schedIn before configure() is a safe no-op");
    Samd21::UsartDriverTester tester;
    tester.testSchedInUnconfigured();
}

TEST(Unconfigured, ActiveIn) {
    COMMENT("activeIn before configure() is a safe no-op");
    Samd21::UsartDriverTester tester;
    tester.testActiveInUnconfigured();
}

// ----------------------------------------------------------------------
// Trigger-source mapping
// ----------------------------------------------------------------------

TEST(TriggerSource, Mapping) {
    COMMENT("each SERCOM maps to its matching RX DMA trigger source");
    Samd21::UsartDriverTester tester;
    tester.testTriggerSourceMapping();
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

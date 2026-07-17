// ======================================================================
// \title  FramerTestMain.cpp
// \author tumbar
// \brief  cpp file for Framer component test main function
// ======================================================================

#include "FramerTester.hpp"
#include "STest/testing.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, SinglePacket) {
    // Test basic single packet framing
    // FRAMER-001: Shall frame individual ComBuffers with FPrime protocol
    Samd21::FramerTester tester;
    tester.testSinglePacket();
}

TEST(Nominal, MultiPacketAccumulation) {
    // Test accumulation of multiple small packets into one frame
    // FRAMER-002: Shall accumulate multiple ComBuffers in a single TX buffer
    Samd21::FramerTester tester;
    tester.testMultiPacketAccumulation();
}

TEST(Nominal, BufferOverflow) {
    // Test automatic flush when buffer would overflow
    // FRAMER-003: Shall automatically flush buffer when next ComBuffer would overflow
    Samd21::FramerTester tester;
    tester.testBufferOverflow();
}

TEST(Nominal, DoubleBufferLifecycle) {
    // Test double-buffer send and return lifecycle
    // FRAMER-004: Shall manage two TX buffers for overlapped send/fill operations
    Samd21::FramerTester tester;
    tester.testDoubleBufferLifecycle();
}

TEST(OffNominal, Backpressure) {
    // Test packet drops under backpressure (both buffers in flight)
    // FRAMER-005: Shall drop packets when both buffers are transmitting
    Samd21::FramerTester tester;
    tester.testBackpressure();
}

TEST(Nominal, SchedInFlush) {
    // Test periodic flush triggered by schedIn port
    // FRAMER-006: Shall flush accumulated packets when schedIn is invoked
    Samd21::FramerTester tester;
    tester.testSchedInFlush();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

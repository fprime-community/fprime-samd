// ======================================================================
// \title  FramerTestMain.cpp
// \author tumbar
// \brief  cpp file for Framer component test main function
// ======================================================================

#include "FramerTester.hpp"
#include "STest/Random/Random.hpp"
#include "STest/testing.hpp"

TEST(Nominal, SinglePacket) {
    // Test basic single packet framing
    Samd21::FramerTester tester;
    tester.testSinglePacket();
}

TEST(Nominal, MultiPacketAccumulation) {
    // Test accumulation of multiple small packets into one frame
    Samd21::FramerTester tester;
    tester.testMultiPacketAccumulation();
}

TEST(Nominal, BufferOverflow) {
    // Test automatic flush when buffer would overflow
    Samd21::FramerTester tester;
    tester.testBufferOverflow();
}

TEST(Nominal, DoubleBufferLifecycle) {
    // Test double-buffer send and return lifecycle
    Samd21::FramerTester tester;
    tester.testDoubleBufferLifecycle();
}

TEST(OffNominal, Backpressure) {
    // Test packet drops under backpressure (both buffers in flight)
    Samd21::FramerTester tester;
    tester.testBackpressure();
}

TEST(Nominal, SchedInFlush) {
    // Test periodic flush triggered by schedIn port
    Samd21::FramerTester tester;
    tester.testSchedInFlush();
}

TEST(Nominal, FrameFormat) {
    Samd21::FramerTester tester;
    tester.testFrameFormat();
}

TEST(Nominal, GroundDeframes) {
    Samd21::FramerTester tester;
    tester.testGroundDeframes();
}

TEST(Nominal, EmptySchedIn) {
    Samd21::FramerTester tester;
    tester.testEmptySchedIn();
}

TEST(OffNominal, DroppedPacketTelemetry) {
    Samd21::FramerTester tester;
    tester.testDroppedPacketTelemetry();
}

TEST(OffNominal, BeforeConnected) {
    Samd21::FramerTester tester;
    tester.testBeforeConnected();
}

TEST(OffNominal, ReturnWithError) {
    Samd21::FramerTester tester;
    tester.testReturnWithError();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

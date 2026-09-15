// ======================================================================
// \title  TmFramerTestMain.cpp
// \brief  cpp file for TmFramer component test main function
// ======================================================================

#include "TmFramerTester.hpp"
#include "STest/testing.hpp"
#include "STest/Random/Random.hpp"

TEST(Nominal, NominalFraming) {
    Samd21::TmFramerTester tester;
    tester.testNominalFraming();
}

TEST(Nominal, MultiPacketAccumulation) {
    Samd21::TmFramerTester tester;
    tester.testMultiPacketAccumulation();
}

TEST(Nominal, DeadZoneAvoidance) {
    Samd21::TmFramerTester tester;
    tester.testDeadZoneAvoidance();
}

TEST(Nominal, BufferOverflow) {
    Samd21::TmFramerTester tester;
    tester.testBufferOverflow();
}

TEST(Nominal, DoubleBufferLifecycle) {
    Samd21::TmFramerTester tester;
    tester.testDoubleBufferLifecycle();
}

TEST(OffNominal, Backpressure) {
    Samd21::TmFramerTester tester;
    tester.testBackpressure();
}

TEST(Nominal, SchedInFlush) {
    Samd21::TmFramerTester tester;
    tester.testSchedInFlush();
}

TEST(Nominal, PerApidSequenceCounts) {
    Samd21::TmFramerTester tester;
    tester.testPerApidSequenceCounts();
}

TEST(Nominal, FrameCountWrapAround) {
    Samd21::TmFramerTester tester;
    tester.testFrameCountWrapAround();
}

TEST(OffNominal, UnexpectedBufferReturn) {
    Samd21::TmFramerTester tester;
    tester.testUnexpectedBufferReturn();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

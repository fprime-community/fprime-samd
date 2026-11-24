// ======================================================================
// \title  PassiveCyclerTestMain.cpp
// \author tumbar
// \brief  cpp file for PassiveCycler component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/PassiveCycler/test/ut/PassiveCyclerTester.hpp"

TEST(Nominal, testIdle) {
    Svc::PassiveCyclerTester tester;
    tester.testIdle();
}

TEST(Nominal, testRepeatUntilIdle) {
    Svc::PassiveCyclerTester tester;
    tester.testRepeatUntilIdle();
}

TEST(Nominal, testLastPortWorks) {
    Svc::PassiveCyclerTester tester;
    tester.testLastPortWorks();
}

TEST(Nominal, testRandomWork) {
    Svc::PassiveCyclerTester tester;
    tester.testRandomWork();
}

TEST(Nominal, testUnconnectedPorts) {
    Svc::PassiveCyclerTester tester;
    tester.testUnconnectedPorts();
}

TEST(OffNominal, testNoPortsConnected) {
    Svc::PassiveCyclerTester tester;
    tester.testNoPortsConnected();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

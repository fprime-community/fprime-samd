// ======================================================================
// \title  PassiveEventManagerTestMain.cpp
// \author tumbar
// \brief  cpp file for PassiveEventManager component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/PassiveEventManager/test/ut/PassiveEventManagerTester.hpp"

TEST(Nominal, testPacketLayout) {
    Samd21::PassiveEventManagerTester tester;
    tester.testPacketLayout();
}

TEST(Nominal, testEverySeverity) {
    Samd21::PassiveEventManagerTester tester;
    tester.testEverySeverity();
}

TEST(Nominal, testArgumentSizes) {
    Samd21::PassiveEventManagerTester tester;
    tester.testArgumentSizes();
}

TEST(Nominal, testFatalAnnounced) {
    Samd21::PassiveEventManagerTester tester;
    tester.testFatalAnnounced();
}

TEST(Nominal, testFatalPacketBeforeAnnounce) {
    Samd21::PassiveEventManagerTester tester;
    tester.testFatalPacketBeforeAnnounce();
}

TEST(Random, testRandomEvents) {
    Samd21::PassiveEventManagerTester tester;
    tester.testRandomEvents();
}

TEST(OffNominal, testOversizedEventDropped) {
    Samd21::PassiveEventManagerTester tester;
    tester.testOversizedEventDropped();
}

TEST(OffNominal, testOversizedFatalAnnounced) {
    Samd21::PassiveEventManagerTester tester;
    tester.testOversizedFatalAnnounced();
}

TEST(OffNominal, testUnconnectedOutputs) {
    Samd21::PassiveEventManagerTester tester;
    tester.testUnconnectedOutputs();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

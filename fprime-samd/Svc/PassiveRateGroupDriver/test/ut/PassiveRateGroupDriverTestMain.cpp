// ======================================================================
// \title  PassiveRateGroupDriverTestMain.cpp
// \author tumbar
// \brief  cpp file for PassiveRateGroupDriver component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Svc/PassiveRateGroupDriver/test/ut/PassiveRateGroupDriverTester.hpp"

TEST(Nominal, testDeploymentDividers) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testDeploymentDividers();
}

TEST(Nominal, testOffsets) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testOffsets();
}

TEST(Nominal, testZeroDivisorDisabled) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testZeroDivisorDisabled();
}

TEST(Nominal, testUnconnectedPortSkipped) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testUnconnectedPortSkipped();
}

TEST(Nominal, testRollover) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testRollover();
}

TEST(Nominal, testReconfigure) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testReconfigure();
}

TEST(Nominal, testCycleStartTimestamp) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testCycleStartTimestamp();
}

TEST(Nominal, testCycleTimeTelemetry) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testCycleTimeTelemetry();
}

TEST(OffNominal, testNotConfiguredAsserts) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testNotConfiguredAsserts();
}

TEST(OffNominal, testOffsetNotBelowDivisorAsserts) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testOffsetNotBelowDivisorAsserts();
}

TEST(OffNominal, testRolloverOverflowAsserts) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testRolloverOverflowAsserts();
}

TEST(Nominal, testLargeCommonDivisors) {
    Samd21::PassiveRateGroupDriverTester tester;
    tester.testLargeCommonDivisors();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

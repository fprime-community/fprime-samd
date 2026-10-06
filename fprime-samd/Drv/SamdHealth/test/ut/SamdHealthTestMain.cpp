// ======================================================================
// \title  SamdHealthTestMain.cpp
// \author tumbar
// \brief  cpp file for SamdHealth component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/SamdHealth/test/ut/SamdHealthTester.hpp"

TEST(Nominal, testEmitSystemInfoReportsResetReason) {
    Samd21::SamdHealthTester tester;
    tester.testEmitSystemInfoReportsResetReason();
}

TEST(Nominal, testCommitStampsAreReported) {
    Samd21::SamdHealthTester tester;
    tester.testCommitStampsAreReported();
}

TEST(Nominal, testHardwareQueriedOncePerCommand) {
    Samd21::SamdHealthTester tester;
    tester.testHardwareQueriedOncePerCommand();
}

TEST(Nominal, testRepeatedEmit) {
    Samd21::SamdHealthTester tester;
    tester.testRepeatedEmit();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

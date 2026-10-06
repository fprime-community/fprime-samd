// ======================================================================
// \title  SystemInfoTestMain.cpp
// \author tumbar
// \brief  cpp file for SystemInfo component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/SystemInfo/test/ut/SystemInfoTester.hpp"

TEST(Nominal, testEmitSystemInfoReportsResetReason) {
    Samd21::SystemInfoTester tester;
    tester.testEmitSystemInfoReportsResetReason();
}

TEST(Nominal, testCommitStampsAreReported) {
    Samd21::SystemInfoTester tester;
    tester.testCommitStampsAreReported();
}

TEST(Nominal, testHardwareQueriedOncePerCommand) {
    Samd21::SystemInfoTester tester;
    tester.testHardwareQueriedOncePerCommand();
}

TEST(Nominal, testRepeatedEmit) {
    Samd21::SystemInfoTester tester;
    tester.testRepeatedEmit();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

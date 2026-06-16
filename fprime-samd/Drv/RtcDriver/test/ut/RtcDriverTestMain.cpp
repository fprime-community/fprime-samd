// ======================================================================
// \title  RtcDriverTestMain.cpp
// \author tumbar
// \brief  cpp file for RtcDriver component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/RtcDriver/test/ut/RtcDriverTester.hpp"

TEST(Nominal, testConfigure) {
    // COMMENT("Test RTC configuration with various clock sources and rates");
    Samd21::RtcDriverTester tester;
    tester.testConfigure();
}

TEST(Nominal, testEnable) {
    // COMMENT("Test RTC enable functionality");
    Samd21::RtcDriverTester tester;
    tester.testEnable();
}

TEST(Nominal, testCycle) {
    // COMMENT("Test single RTC cycle");
    Samd21::RtcDriverTester tester;
    tester.testCycle();
}

TEST(OffNominal, testCycleOverrun) {
    // COMMENT("Test cycle overrun detection");
    Samd21::RtcDriverTester tester;
    tester.testCycleOverrun();
}

TEST(Nominal, testMultipleCycles) {
    // COMMENT("Test multiple consecutive RTC cycles");
    Samd21::RtcDriverTester tester;
    tester.testMultipleCycles();
}

TEST(Nominal, testTimeNow) {
    // COMMENT("Test Os::RawTime implementation");
    Samd21::RtcDriverTester tester;
    tester.testTimeNow();
}

TEST(Nominal, testTimeNowUnconfigured) {
    // COMMENT("Test time query before RTC configuration");
    Samd21::RtcDriverTester tester;
    tester.testTimeNowUnconfigured();
}

TEST(Nominal, testTimeNowFunction) {
    // COMMENT("Test timeNow() function with various RTC states");
    Samd21::RtcDriverTester tester;
    tester.testTimeNowFunction();
}

TEST(Nominal, testSamd21RawTimeNow) {
    // COMMENT("Test Samd21RawTime::now() method");
    Samd21::RtcDriverTester tester;
    tester.testSamd21RawTimeNow();
}

TEST(Nominal, testSamd21RawTimeSerialization) {
    // COMMENT("Test Samd21RawTime serialization/deserialization");
    Samd21::RtcDriverTester tester;
    tester.testSamd21RawTimeSerialization();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

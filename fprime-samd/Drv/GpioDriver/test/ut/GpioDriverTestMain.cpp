// ======================================================================
// \title  GpioDriverTestMain.cpp
// \author tumbar
// \brief  cpp file for GpioDriver component test main function
// ======================================================================

#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/GpioDriver/test/ut/GpioDriverTester.hpp"

TEST(Nominal, testConfigureOutput) {
    // COMMENT("configure() forwards output configuration to the HAL");
    Samd21::GpioDriverTester tester;
    tester.testConfigureOutput();
}

TEST(Nominal, testConfigureInput) {
    // COMMENT("configure() forwards input configuration and pull selection to the HAL");
    Samd21::GpioDriverTester tester;
    tester.testConfigureInput();
}

TEST(Nominal, testConfigureAllPins) {
    // COMMENT("configure() supports every group/pin combination");
    Samd21::GpioDriverTester tester;
    tester.testConfigureAllPins();
}

TEST(Nominal, testWriteNominal) {
    // COMMENT("gpioWrite on a configured output pin drives the HAL");
    Samd21::GpioDriverTester tester;
    tester.testWriteNominal();
}

TEST(Nominal, testReadNominal) {
    // COMMENT("gpioRead on a configured input pin reads the HAL");
    Samd21::GpioDriverTester tester;
    tester.testReadNominal();
}

TEST(OffNominal, testWriteUnconfigured) {
    // COMMENT("gpioWrite before configure() returns NOT_OPENED");
    Samd21::GpioDriverTester tester;
    tester.testWriteUnconfigured();
}

TEST(OffNominal, testReadUnconfigured) {
    // COMMENT("gpioRead before configure() returns NOT_OPENED");
    Samd21::GpioDriverTester tester;
    tester.testReadUnconfigured();
}

TEST(OffNominal, testWriteWrongMode) {
    // COMMENT("gpioWrite on an input pin returns INVALID_MODE");
    Samd21::GpioDriverTester tester;
    tester.testWriteWrongMode();
}

TEST(OffNominal, testReadWrongMode) {
    // COMMENT("gpioRead on an output pin returns INVALID_MODE");
    Samd21::GpioDriverTester tester;
    tester.testReadWrongMode();
}

int main(int argc, char** argv) {
    // Seed random number generator for STest
    STest::Random::seed();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

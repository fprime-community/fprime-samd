// ======================================================================
// \title  AdcDriverTestMain.cpp
// \author crsmith
// \brief  cpp file for AdcDriver component test main function
// ======================================================================

#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"
#include "fprime-samd/Drv/AdcDriver/test/ut/AdcDriverTester.hpp"

// ----------------------------------------------------------------------
// configure()
// ----------------------------------------------------------------------

TEST(Configure, Configure) {
    COMMENT("configure() initializes ADC with all parameters and calls sync waits");
    Samd21::AdcDriverTester tester;
    tester.testConfigure();
}

TEST(Configure, AllReferences) {
    COMMENT("configure() works with all voltage reference options");
    Samd21::AdcDriverTester tester;
    tester.testConfigureAllReferences();
}

TEST(Configure, AllResolutions) {
    COMMENT("configure() works with all resolution options (8/10/12-bit)");
    Samd21::AdcDriverTester tester;
    tester.testConfigureAllResolutions();
}

TEST(Configure, AllSampleCounts) {
    COMMENT("configure() works with all hardware averaging sample counts");
    Samd21::AdcDriverTester tester;
    tester.testConfigureAllSampleCounts();
}

TEST(Configure, AllGains) {
    COMMENT("configure() works with all gain options (1X and DIV2)");
    Samd21::AdcDriverTester tester;
    tester.testConfigureAllGains();
}

// ----------------------------------------------------------------------
// configureChannel()
// ----------------------------------------------------------------------

TEST(ConfigureChannel, Comprehensive) {
    COMMENT("configureChannel() handles external channels, internal channels (TEMP, BANDGAP), and SCALEDIOVCC");
    Samd21::AdcDriverTester tester;
    tester.testConfigureChannelNominal();
}

// ----------------------------------------------------------------------
// ADC conversion: error cases (readAdc validation)
// ----------------------------------------------------------------------

TEST(Conversion, NotConfigured) {
    COMMENT("readAdc() before configure() returns NOT_CONFIGURED");
    Samd21::AdcDriverTester tester;
    tester.testConversionNotConfigured();
}

TEST(Conversion, ChannelNotConfigured) {
    COMMENT("readAdc() on unconfigured channel returns INVALID_CHANNEL");
    Samd21::AdcDriverTester tester;
    tester.testConversionChannelNotConfigured();
}

TEST(Conversion, Busy) {
    COMMENT("readAdc() while conversion pending returns BUSY");
    Samd21::AdcDriverTester tester;
    tester.testConversionBusy();
}

// ----------------------------------------------------------------------
// ADC conversion: completion flow (full end-to-end)
// ----------------------------------------------------------------------

TEST(Conversion, Completion) {
    COMMENT("Full conversion flow: readAdc → ISR → activeIn → result delivery");
    Samd21::AdcDriverTester tester;
    tester.testConversionCompletion();
}

TEST(Conversion, MultipleChannels) {
    COMMENT("Sequential conversions across multiple channels with result delivery");
    Samd21::AdcDriverTester tester;
    tester.testConversionMultipleChannels();
}

TEST(Conversion, ResultBoundaries) {
    COMMENT("Conversion handling across full ADC range (0, mid, max)");
    Samd21::AdcDriverTester tester;
    tester.testConversionResultBoundaries();
}

// ----------------------------------------------------------------------
// ADC conversion: edge cases
// ----------------------------------------------------------------------

TEST(Conversion, BeforeComplete) {
    COMMENT("activeIn() called before ISR completes does not deliver premature result");
    Samd21::AdcDriverTester tester;
    tester.testConversionBeforeComplete();
}

TEST(Conversion, MultipleactiveInIdle) {
    COMMENT("Multiple activeIn() calls while idle do not produce spurious results");
    Samd21::AdcDriverTester tester;
    tester.testConversionMultipleactiveInIdle();
}

// ----------------------------------------------------------------------
// ISR / handleInterrupt()
// ----------------------------------------------------------------------

TEST(Interrupt, ResultReady) {
    COMMENT("handleInterrupt() processes RESRDY flag and stores result");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptResultReady();
}

TEST(Interrupt, Overrun) {
    COMMENT("handleInterrupt() handles OVERRUN flag and still delivers result");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptOverrun();
}

TEST(Interrupt, BothFlags) {
    COMMENT("handleInterrupt() correctly handles both OVERRUN and RESRDY flags");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptBothFlags();
}

TEST(Interrupt, NoFlags) {
    COMMENT("handleInterrupt() with no flags set (spurious interrupt) is handled safely");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptNoFlags();
}

TEST(Interrupt, MultipleTimes) {
    COMMENT("handleInterrupt() firing multiple times with same result");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptMultipleTimes();
}

TEST(Interrupt, WithoutPendingConversion) {
    COMMENT("handleInterrupt() without pending conversion (spurious) does not deliver result");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptWithoutPendingConversion();
}

TEST(Interrupt, SpuriousWithOverrun) {
    COMMENT("handleInterrupt() spurious interrupt with overrun flag clears flags and doesn't corrupt state");
    Samd21::AdcDriverTester tester;
    tester.testHandleInterruptSpuriousWithOverrun();
}

int main(int argc, char** argv) {
    STest::Random::seed();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

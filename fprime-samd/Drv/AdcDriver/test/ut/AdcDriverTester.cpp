// ======================================================================
// \title  AdcDriverTester.cpp
// \author crsmith
// \brief  cpp file for AdcDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/test/ut/AdcDriverTester.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

AdcDriverTester::AdcDriverTester()
    : AdcDriverGTestBase("AdcDriverTester", AdcDriverTester::MAX_HISTORY_SIZE), component("AdcDriver") {
    this->initComponents();
    this->connectPorts();
    AdcHardware::resetStubState();
}

AdcDriverTester::~AdcDriverTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

AdcHardware::StubState& AdcDriverTester::stub() {
    return AdcHardware::getStubState();
}

void AdcDriverTester::resetTest() {
    this->clearHistory();
    AdcHardware::resetStubState();
}

void AdcDriverTester::resetSingleton() {
    // Clear the singleton instance pointer to allow reconfiguration in tests
    // This simulates destroying and recreating the component
    Samd21::AdcDriver::s_instance = nullptr;
}

void AdcDriverTester::configureStandard() {
    this->component.configure(AdcDriver::VoltageReference::INT1V, AdcDriver::Resolution::RES_12BIT,
                              AdcDriver::SampleCount::SAMPLES_1, 5, AdcDriver::Gain::GAIN_1X);
}

void AdcDriverTester::simulateConversionComplete(U32 result) {
    // Inject the result into the stub
    this->stub().result = result;
    this->stub().result_ready = true;

    // Call the interrupt handler (simulates hardware firing the interrupt)
    this->component.handleInterrupt();
}

void AdcDriverTester::simulateOverrun() {
    // Simulate both overrun and result ready flags set
    this->stub().overrun = true;
    this->stub().result_ready = true;
    this->stub().result = 0xFFF;  // Some value

    // Call the interrupt handler
    this->component.handleInterrupt();
}

void AdcDriverTester::assertAdcResult(FwSizeType size,
                                      FwIndexType index,
                                      U32 expectedValue,
                                      Samd21::AdcStatus expectedStatus) {
    ASSERT_from_adcResult_SIZE(size);
    ASSERT_from_adcResult(index, expectedValue, expectedStatus);
}

// ----------------------------------------------------------------------
// configure()
// ----------------------------------------------------------------------

void AdcDriverTester::testConfigure() {
    this->resetTest();

    // Configure with non-default parameters to verify all parameter plumbing
    this->component.configure(AdcDriver::VoltageReference::INTVCC1, AdcDriver::Resolution::RES_10BIT, AdcDriver::SampleCount::SAMPLES_16, 10,
                              AdcDriver::Gain::GAIN_DIV2);

    // Verify hardware was configured exactly once with the requested parameters
    ASSERT_TRUE(this->stub().configured);
    ASSERT_EQ(this->stub().configure_count, 1U);
    ASSERT_EQ(static_cast<U8>(this->stub().ref), static_cast<U8>(AdcDriver::VoltageReference::INTVCC1));
    ASSERT_EQ(static_cast<U8>(this->stub().res), static_cast<U8>(AdcDriver::Resolution::RES_10BIT));
    ASSERT_EQ(static_cast<U8>(this->stub().samples), static_cast<U8>(AdcDriver::SampleCount::SAMPLES_16));
    ASSERT_EQ(this->stub().samplingTime, 10U);
    ASSERT_EQ(static_cast<U8>(this->stub().gain), static_cast<U8>(AdcDriver::Gain::GAIN_DIV2));

    // Verify HAL calls the sync waits (prevents hanging on hardware misconfiguration)
    ASSERT_GT(this->stub().waitForGclkSync_calls, 0U);
    ASSERT_GT(this->stub().waitForAdcSync_calls, 0U);
}

// ----------------------------------------------------------------------
// configureChannel()
// ----------------------------------------------------------------------

void AdcDriverTester::testConfigureChannelNominal() {
    this->resetTest();
    this->configureStandard();
    this->clearHistory();

    // Configure external ADC channels (AIN0-AIN9) - don't require HAL configuration
    for (U32 i = 0; i < 5; i++) {
        this->component.configureChannel(i, static_cast<AdcDriver::AdcChannel>(i));  // AIN0-AIN4
    }
    // External channels should not trigger HAL calls
    ASSERT_EQ(this->stub().configureChannel_count, 0U);

    // Configure internal channels that require HAL setup (TEMP, BANDGAP)
    this->component.configureChannel(5, AdcDriver::AdcChannel::TEMP);
    ASSERT_EQ(this->stub().configureChannel_count, 1U);
    ASSERT_EQ(static_cast<U8>(this->stub().last_configured_channel), static_cast<U8>(AdcDriver::AdcChannel::TEMP));

    this->component.configureChannel(6, AdcDriver::AdcChannel::BANDGAP);
    ASSERT_EQ(this->stub().configureChannel_count, 2U);
    ASSERT_EQ(static_cast<U8>(this->stub().last_configured_channel), static_cast<U8>(AdcDriver::AdcChannel::BANDGAP));

    // Configure internal channel that doesn't require HAL setup (SCALEDIOVCC)
    this->component.configureChannel(7, AdcDriver::AdcChannel::SCALEDIOVCC);
    ASSERT_EQ(this->stub().configureChannel_count, 2U);  // Should still be 2 (no new HAL call)
}

// ----------------------------------------------------------------------
// ADC conversion flow (readAdc + activeIn)
// ----------------------------------------------------------------------

void AdcDriverTester::testConversionCompletion() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Request an ADC conversion on port 0
    Samd21::AdcStatus status = this->invoke_to_readAdc(0);

    // Should return OK status
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);

    // Hardware operations were called
    ASSERT_EQ(this->stub().selectChannel_count, 1U);
    ASSERT_EQ(static_cast<U8>(this->stub().selected_channel), static_cast<U8>(AdcDriver::AdcChannel::AIN0));
    ASSERT_EQ(this->stub().startConversion_count, 1U);

    // No result yet (conversion is asynchronous)
    ASSERT_from_adcResult_SIZE(0);

    // Simulate conversion complete
    this->simulateConversionComplete(1024);

    // Process the result via activeIn
    this->invoke_to_activeIn(0, 0);

    // Result should be delivered via adcResult output port
    this->assertAdcResult(1, 0, 1024U, Samd21::AdcStatus::ADC_OK);
}

void AdcDriverTester::testConversionNotConfigured() {
    this->resetTest();
    // Don't call configureStandard() - ADC peripheral not configured

    // Attempt to read without configuration
    Samd21::AdcStatus status = this->invoke_to_readAdc(0);

    // Should return NOT_CONFIGURED status
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_NOT_CONFIGURED);
    ASSERT_from_adcResult_SIZE(0);

    // No hardware operations should have been called
    ASSERT_EQ(this->stub().selectChannel_count, 0U);
    ASSERT_EQ(this->stub().startConversion_count, 0U);
}

void AdcDriverTester::testConversionChannelNotConfigured() {
    this->resetTest();
    this->configureStandard();
    // Configure channel 0 but not channel 1
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Attempt to read from unconfigured channel 1
    Samd21::AdcStatus status = this->invoke_to_readAdc(1);

    // Should return INVALID_CHANNEL status
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_INVALID_CHANNEL);
    ASSERT_from_adcResult_SIZE(0);

    // No hardware operations for the failed read
    ASSERT_EQ(this->stub().selectChannel_count, 0U);
    ASSERT_EQ(this->stub().startConversion_count, 0U);
}

void AdcDriverTester::testConversionBusy() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion on port 0
    Samd21::AdcStatus status1 = this->invoke_to_readAdc(0);
    ASSERT_EQ(status1, Samd21::AdcStatus::ADC_OK);
    ASSERT_EQ(this->stub().startConversion_count, 1U);

    // Attempt a second conversion while the first is still pending
    Samd21::AdcStatus status2 = this->invoke_to_readAdc(0);

    // Should return BUSY status
    ASSERT_EQ(status2, Samd21::AdcStatus::ADC_BUSY);
    ASSERT_from_adcResult_SIZE(0);

    // No additional hardware start should have been called
    ASSERT_EQ(this->stub().startConversion_count, 1U);
}

void AdcDriverTester::testConversionMultipleChannels() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->component.configureChannel(1, AdcDriver::AdcChannel::AIN1);
    this->clearHistory();

    // Read from channel 0
    Samd21::AdcStatus status = this->invoke_to_readAdc(0);
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);

    // Simulate conversion complete
    this->simulateConversionComplete(1024);

    // Process the result via activeIn
    this->invoke_to_activeIn(0, 0);

    // Result should be delivered via adcResult output port
    this->assertAdcResult(1, 0, 1024U, Samd21::AdcStatus::ADC_OK);

    // Now read from channel 1
    status = this->invoke_to_readAdc(1);
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);

    // Verify the correct channel was selected
    ASSERT_EQ(static_cast<U8>(this->stub().selected_channel), static_cast<U8>(AdcDriver::AdcChannel::AIN1));

    // Simulate conversion complete with different value
    this->simulateConversionComplete(2048);

    // Process the result
    this->invoke_to_activeIn(0, 0);

    // Second result should be delivered
    this->assertAdcResult(2, 1, 2048U, Samd21::AdcStatus::ADC_OK);
}

// ----------------------------------------------------------------------
// ISR / handleInterrupt()
// ----------------------------------------------------------------------

void AdcDriverTester::testHandleInterruptResultReady() {
    // Technically repeats testConversionCompletion
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion
    this->invoke_to_readAdc(0);

    // Simulate result ready interrupt
    const U32 result = 3072;
    this->simulateConversionComplete(result);

    // Process via activeIn
    this->invoke_to_activeIn(0, 0);

    // Result should be delivered with OK status
    this->assertAdcResult(1, 0, result, Samd21::AdcStatus::ADC_OK);
}

void AdcDriverTester::testHandleInterruptOverrun() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion
    this->invoke_to_readAdc(0);

    // Simulate an overrun condition (data ready before previous was read)
    this->simulateOverrun();

    // Process via activeIn - result should be delivered with OVERRUN status
    this->invoke_to_activeIn(0, 0);

    // Verify result delivered with ADC_OVERRUN status
    this->assertAdcResult(1, 0, 0xFFF, Samd21::AdcStatus::ADC_OVERRUN);

    // Verify overrun flag was cleared in hardware
    ASSERT_GT(this->stub().clearOverrun_count, 0U);
}

void AdcDriverTester::testHandleInterruptBothFlags() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion
    this->invoke_to_readAdc(0);

    // Simulate both OVERRUN and RESRDY set simultaneously
    this->stub().result = 4095;
    this->stub().overrun = true;
    this->stub().result_ready = true;
    this->component.handleInterrupt();

    // Process via activeIn
    this->invoke_to_activeIn(0, 0);

    // Result should be delivered with OVERRUN status
    this->assertAdcResult(1, 0, 4095U, Samd21::AdcStatus::ADC_OVERRUN);

    // Overrun flag should have been explicitly cleared
    // (result_ready auto-clears when readResult() is called)
    ASSERT_GT(this->stub().clearOverrun_count, 0U);
}

// ----------------------------------------------------------------------
// configure() - extended tests
// ----------------------------------------------------------------------

void AdcDriverTester::testConfigureAllReferences() {
    this->resetTest();

    // Test all voltage reference options to achieve line coverage in HAL switch statement
    AdcDriver::VoltageReference refs[] = {
        AdcDriver::VoltageReference::INT1V, AdcDriver::VoltageReference::INTVCC0, AdcDriver::VoltageReference::INTVCC1,
        AdcDriver::VoltageReference::VREFA, AdcDriver::VoltageReference::VREFB};

    for (auto ref : refs) {
        this->resetTest();
        this->resetSingleton();  // Clear s_instance to allow configure() again
        this->component.configure(ref, AdcDriver::Resolution::RES_12BIT, AdcDriver::SampleCount::SAMPLES_1, 5,
                                  AdcDriver::Gain::GAIN_1X);

        ASSERT_TRUE(this->stub().configured);
        ASSERT_EQ(static_cast<U8>(this->stub().ref), static_cast<U8>(ref));
    }
}

void AdcDriverTester::testConfigureAllResolutions() {
    this->resetTest();

    // Test all resolution options (8, 10, 12 bit) for line coverage
    AdcDriver::Resolution resolutions[] = {AdcDriver::Resolution::RES_8BIT, AdcDriver::Resolution::RES_10BIT,
                                            AdcDriver::Resolution::RES_12BIT};

    for (auto res : resolutions) {
        this->resetTest();
        this->resetSingleton();  // Clear s_instance to allow configure() again
        this->component.configure(AdcDriver::VoltageReference::INT1V, res, AdcDriver::SampleCount::SAMPLES_1, 5,
                                  AdcDriver::Gain::GAIN_1X);

        ASSERT_TRUE(this->stub().configured);
        ASSERT_EQ(static_cast<U8>(this->stub().res), static_cast<U8>(res));
    }
}

void AdcDriverTester::testConfigureAllSampleCounts() {
    this->resetTest();

    // Test all sample count options (1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024) for line coverage
    AdcDriver::SampleCount counts[] = {
        AdcDriver::SampleCount::SAMPLES_1,  AdcDriver::SampleCount::SAMPLES_2,   AdcDriver::SampleCount::SAMPLES_4,
        AdcDriver::SampleCount::SAMPLES_8,  AdcDriver::SampleCount::SAMPLES_16,  AdcDriver::SampleCount::SAMPLES_32,
        AdcDriver::SampleCount::SAMPLES_64, AdcDriver::SampleCount::SAMPLES_128, AdcDriver::SampleCount::SAMPLES_256,
        AdcDriver::SampleCount::SAMPLES_512, AdcDriver::SampleCount::SAMPLES_1024};

    for (auto count : counts) {
        this->resetTest();
        this->resetSingleton();  // Clear s_instance to allow configure() again
        this->component.configure(AdcDriver::VoltageReference::INT1V, AdcDriver::Resolution::RES_12BIT, count, 5, AdcDriver::Gain::GAIN_1X);

        ASSERT_TRUE(this->stub().configured);
        ASSERT_EQ(static_cast<U8>(this->stub().samples), static_cast<U8>(count));
    }
}

void AdcDriverTester::testConfigureAllGains() {
    this->resetTest();

    // Test both gain options (1X and DIV2) for line coverage
    AdcDriver::Gain gains[] = {AdcDriver::Gain::GAIN_1X, AdcDriver::Gain::GAIN_DIV2};

    for (U32 i = 0; i < 2; i++) {
        AdcDriver::Gain gain = gains[i];
        this->resetTest();
        this->resetSingleton();  // Clear s_instance to allow configure() again
        this->component.configure(AdcDriver::VoltageReference::INT1V, AdcDriver::Resolution::RES_12BIT, AdcDriver::SampleCount::SAMPLES_1, 5, gain);

        ASSERT_TRUE(this->stub().configured);
        ASSERT_EQ(static_cast<U8>(this->stub().gain), static_cast<U8>(gain));

        this->component.configureChannel(i, AdcDriver::AdcChannel::AIN0);
        Samd21::AdcStatus status = this->invoke_to_readAdc(i);
        ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);
        ASSERT_EQ(this->stub().selected_gain, static_cast<U8>(gain));

        this->simulateConversionComplete(0);
        this->invoke_to_activeIn(0, 0);
    }
}

// ----------------------------------------------------------------------
// readAdc() - extended tests
// ----------------------------------------------------------------------

void AdcDriverTester::testConversionResultBoundaries() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Test various result values across the 12-bit range
    U32 testValues[] = {0, 1, 255, 256, 1023, 1024, 2047, 2048, 4094, 4095};

    for (U32 i = 0; i < 10; i++) {
        U32 value = testValues[i];

        Samd21::AdcStatus status = this->invoke_to_readAdc(0);
        ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);

        this->simulateConversionComplete(value);
        this->invoke_to_activeIn(0, 0);

        // Check that we got the correct result for this iteration with OK status
        this->assertAdcResult(i + 1, i, value, Samd21::AdcStatus::ADC_OK);
    }
}

// ----------------------------------------------------------------------
// ADC conversion: edge cases
// ----------------------------------------------------------------------

void AdcDriverTester::testConversionBeforeComplete() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion
    Samd21::AdcStatus status = this->invoke_to_readAdc(0);
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);

    // Call activeIn before conversion completes (interrupt hasn't fired yet)
    this->invoke_to_activeIn(0, 0);

    // No result should be delivered yet
    ASSERT_from_adcResult_SIZE(0);

    // Now complete the conversion
    this->simulateConversionComplete(2048);
    this->invoke_to_activeIn(0, 0);

    // Now result should be delivered with OK status
    this->assertAdcResult(1, 0, 2048U, Samd21::AdcStatus::ADC_OK);
}

void AdcDriverTester::testConversionMultipleactiveInIdle() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start and complete a conversion
    this->invoke_to_readAdc(0);
    this->simulateConversionComplete(1500);
    this->invoke_to_activeIn(0, 0);

    this->assertAdcResult(1, 0, 1500U, Samd21::AdcStatus::ADC_OK);

    // Call activeIn multiple more times without starting a new conversion
    for (U32 i = 0; i < 5; i++) {
        this->invoke_to_activeIn(0, 0);
    }

    // Should still only have one result
    ASSERT_from_adcResult_SIZE(1);
}

// ----------------------------------------------------------------------
// ISR / handleInterrupt()
// ----------------------------------------------------------------------

void AdcDriverTester::testHandleInterruptNoFlags() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion
    this->invoke_to_readAdc(0);

    // Call interrupt handler with no flags set (spurious interrupt)
    this->stub().overrun = false;
    this->stub().result_ready = false;
    this->component.handleInterrupt();

    // Process via activeIn
    this->invoke_to_activeIn(0, 0);

    // No result should be delivered
    ASSERT_from_adcResult_SIZE(0);
}

void AdcDriverTester::testHandleInterruptMultipleTimes() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Start a conversion
    this->invoke_to_readAdc(0);

    // Simulate interrupt firing multiple times with same result
    for (U32 i = 0; i < 3; i++) {
        this->stub().result = 2000;
        this->stub().result_ready = true;
        this->component.handleInterrupt();
        this->stub().result_ready = false;  // Reset for next iteration
    }

    // Process via activeIn
    this->invoke_to_activeIn(0, 0);

    // Only one result should be delivered with OK status
    this->assertAdcResult(1, 0, 2000U, Samd21::AdcStatus::ADC_OK);
}

void AdcDriverTester::testHandleInterruptWithoutPendingConversion() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Call interrupt handler without starting a conversion (spurious interrupt)
    this->stub().result = 1234;
    this->stub().result_ready = true;
    this->component.handleInterrupt();

    // Process via activeIn
    this->invoke_to_activeIn(0, 0);

    // No result should be delivered (no conversion was requested)
    ASSERT_from_adcResult_SIZE(0);
}

void AdcDriverTester::testHandleInterruptSpuriousWithOverrun() {
    this->resetTest();
    this->configureStandard();
    this->component.configureChannel(0, AdcDriver::AdcChannel::AIN0);
    this->clearHistory();

    // Simulate spurious interrupt with both OVERRUN and RESRDY flags set
    // (hardware wedge scenario - interrupt fires when no conversion was requested)
    this->stub().result = 0xABC;
    this->stub().overrun = true;
    this->stub().result_ready = true;
    this->component.handleInterrupt();

    // Both flags should have been cleared by the spurious interrupt handler
    ASSERT_GT(this->stub().clearOverrun_count, 0U);

    // Process via activeIn
    this->invoke_to_activeIn(0, 0);

    // No result should be delivered (no conversion was requested)
    ASSERT_from_adcResult_SIZE(0);

    // Now start a real conversion to verify the driver still works after spurious interrupt
    Samd21::AdcStatus status = this->invoke_to_readAdc(0);
    ASSERT_EQ(status, Samd21::AdcStatus::ADC_OK);

    // Complete the real conversion
    this->simulateConversionComplete(2048);
    this->invoke_to_activeIn(0, 0);

    // Now the real result should be delivered with OK status
    this->assertAdcResult(1, 0, 2048U, Samd21::AdcStatus::ADC_OK);
}

}  // namespace Samd21

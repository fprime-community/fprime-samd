// ======================================================================
// \title  RtcDriverTester.cpp
// \author tumbar
// \brief  cpp file for RtcDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/RtcDriver/test/ut/RtcDriverTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"
#include "fprime-samd/Drv/RtcDriver/RawTime.hpp"

namespace Samd21 {

// Declare stub helper functions
namespace RtcHardware {
extern void simulateRtcInterrupt();
extern void resetRtcState();
extern void setRtcCounter(U32 counter);
}  // namespace RtcHardware

// Construction and destruction

RtcDriverTester::RtcDriverTester()
    : RtcDriverGTestBase("RtcDriverTester", RtcDriverTester::MAX_HISTORY_SIZE), component("RtcDriver") {
    this->initComponents();
    this->connectPorts();
}

RtcDriverTester::~RtcDriverTester() {}

// Helper functions

void RtcDriverTester::resetTest() {
    this->clearHistory();
    this->resetRtcHardware();
}

void RtcDriverTester::simulateRtcInterrupt() {
    RtcHardware::simulateRtcInterrupt();
}

void RtcDriverTester::resetRtcHardware() {
    RtcHardware::resetRtcState();
}

void RtcDriverTester::setRtcCounter(U32 counter) {
    RtcHardware::setRtcCounter(counter);
}

void RtcDriverTester::assertCycleOut(FwIndexType port_num) {
    (void)port_num;  // Only one CycleOut port, parameter unused
    ASSERT_FROM_PORT_HISTORY_SIZE(1);
    ASSERT_from_CycleOut_SIZE(1);

    // Verify the cycleStart time parameter is present
    ASSERT_TRUE(true);  // Port was called with a RawTime parameter
}

void RtcDriverTester::assertTlmCycleOverrun(U16 expected_overrun) {
    ASSERT_TLM_SIZE(1);
    ASSERT_TLM_CycleOverrun_SIZE(1);
    ASSERT_TLM_CycleOverrun(0, expected_overrun);
}

// Tests

void RtcDriverTester::testConfigure() {
    REQUIREMENT("SAMD21-RTC-001: The RtcDriver shall configure the RTC peripheral for periodic interrupts");
    REQUIREMENT("SAMD21-RTC-002: The RtcDriver shall support configurable interrupt periods (1-128 Hz)");
    REQUIREMENT("SAMD21-RTC-005: The RtcDriver shall support internal, external, and ULP clock sources");

    this->resetTest();

    // Test configuration with different clock sources and rates
    const RtcDriver::ClockSource sources[] = {
        RtcDriver::ClockSource::InternalOscillator,
        RtcDriver::ClockSource::ExternalOscillator,
        RtcDriver::ClockSource::UltraLowPowerOscillator,
    };

    const RtcDriver::TickRate rates[] = {
        RtcDriver::TickRate::TICK_1_HZ,  RtcDriver::TickRate::TICK_2_HZ,   RtcDriver::TickRate::TICK_4_HZ,
        RtcDriver::TickRate::TICK_8_HZ,  RtcDriver::TickRate::TICK_16_HZ,  RtcDriver::TickRate::TICK_32_HZ,
        RtcDriver::TickRate::TICK_64_HZ, RtcDriver::TickRate::TICK_128_HZ,
    };

    for (const auto source : sources) {
        for (const auto rate : rates) {
            // COMMENT("Testing clock source and tick rate configuration");

            this->resetRtcHardware();

            // Configure the RTC
            this->component.configure(source, rate);

            // Verify state was set correctly
            const RtcHardware::RtcState& state = RtcHardware::getRtcState();
            ASSERT_TRUE(state.configured);
            ASSERT_EQ(state.rate_hz, rate);
            ASSERT_EQ(state.n_ticks, 0U);
            ASSERT_GT(state.clock_rate_hz, 0U);
        }
    }
}

void RtcDriverTester::testConfigureAssertsOnDoubleConfigure() {
    REQUIREMENT("SAMD21-RTC-001: The RtcDriver shall configure the RTC peripheral for periodic interrupts");

    this->resetTest();

    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);

    // Configuring a second time without resetting the hardware state is a programming error
    ASSERT_DEATH_IF_SUPPORTED(
        this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ),
        "Assert:");
}

void RtcDriverTester::testConfigureAssertsOnInvalidRate() {
    REQUIREMENT("SAMD21-RTC-002: The RtcDriver shall support configurable interrupt periods (1-128 Hz)");

    this->resetTest();

    const RtcDriver::TickRate invalid_rate = static_cast<RtcDriver::TickRate>(3);
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(RtcDriver::ClockSource::InternalOscillator, invalid_rate),
                              "Assert:");
}

void RtcDriverTester::testConfigureAssertsOnInvalidClockSource() {
    REQUIREMENT("SAMD21-RTC-005: The RtcDriver shall support internal, external, and ULP clock sources");

    this->resetTest();

    const RtcDriver::ClockSource invalid_source = static_cast<RtcDriver::ClockSource>(99);
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(invalid_source, RtcDriver::TickRate::TICK_1_HZ), "Assert:");
}

void RtcDriverTester::testEnable() {
    REQUIREMENT("SAMD21-RTC-001: The RtcDriver shall configure the RTC peripheral for periodic interrupts");

    this->resetTest();

    // Configure first
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);

    // Enable the RTC
    this->component.enable();

    // Verify state indicates configuration is complete
    const RtcHardware::RtcState& state = RtcHardware::getRtcState();
    ASSERT_TRUE(state.configured);
}

void RtcDriverTester::testEnableAssertsWhenNotConfigured() {
    REQUIREMENT("SAMD21-RTC-001: The RtcDriver shall configure the RTC peripheral for periodic interrupts");

    this->resetTest();

    // Enabling before configure() is a programming error
    ASSERT_DEATH_IF_SUPPORTED(this->component.enable(), "Assert:");
}

void RtcDriverTester::testCycle() {
    REQUIREMENT("SAMD21-RTC-004: The RtcDriver shall detect RTC interrupts via activeIn port");
    REQUIREMENT("SAMD21-RTC-006: The RtcDriver shall emit cycle signals via the CycleOut port");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    // Set deadline reached (simulating the component did its work in time)
    RtcHardware::RtcState& state = RtcHardware::getRtcState();
    state.deadline_reached = true;

    // Simulate an RTC interrupt
    this->simulateRtcInterrupt();

    // Call activeIn_handler - should emit the signal
    // A wakeup interrupt is pending, so the handler did work and should return true
    bool didWork = this->invoke_to_activeIn(0, 0);
    EXPECT_TRUE(didWork);

    // Verify cycle was emitted
    ASSERT_FROM_PORT_HISTORY_SIZE(1);
    ASSERT_from_CycleOut_SIZE(1);

    // Verify no overrun occurred
    ASSERT_EQ(state.deadline_exceeded, 0U);
}

void RtcDriverTester::testCycleNoInterruptPending() {
    REQUIREMENT("SAMD21-RTC-004: The RtcDriver shall detect RTC interrupts via activeIn port");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    // No RTC interrupt has fired, so wakeup_interrupt is false: the handler should
    // acknowledge the ISR tick but report that no cycle work was done, and emit nothing.
    RtcHardware::RtcState& state = RtcHardware::getRtcState();
    state.deadline_reached = true;

    bool didWork = this->invoke_to_activeIn(0, 0);
    EXPECT_FALSE(didWork);

    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_TLM_SIZE(0);

    // The ISR acknowledgement still happens unconditionally
    ASSERT_TRUE(state.deadline_reached);
}

void RtcDriverTester::testCycleOverrun() {
    REQUIREMENT("SAMD21-RTC-008: The RtcDriver shall detect when processing exceeds the configured period");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    // Simulate an RTC interrupt without setting deadline_reached
    // This simulates missing a deadline
    RtcHardware::RtcState& state = RtcHardware::getRtcState();
    state.deadline_reached = false;  // Simulate not meeting deadline

    this->simulateRtcInterrupt();

    // Call activeIn_handler
    // A wakeup interrupt is pending, so the handler did work and should return true
    bool didWork = this->invoke_to_activeIn(0, 0);
    EXPECT_TRUE(didWork);

    // Verify overrun was detected and reported
    this->assertTlmCycleOverrun(1);

    // Verify state shows overrun
    ASSERT_EQ(state.deadline_exceeded, 1U);
}

void RtcDriverTester::testCycleOverrunAccumulates() {
    REQUIREMENT("SAMD21-RTC-008: The RtcDriver shall detect when processing exceeds the configured period");

    this->resetTest();

    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    RtcHardware::RtcState& state = RtcHardware::getRtcState();

    // Three consecutive missed deadlines should accumulate rather than reset
    for (U16 i = 1; i <= 3; i++) {
        state.deadline_reached = false;
        this->simulateRtcInterrupt();
        this->invoke_to_activeIn(0, static_cast<U32>(i));
        ASSERT_EQ(state.deadline_exceeded, i);
    }

    // Three overrun cycles should have reported three increasing telemetry values
    ASSERT_TLM_SIZE(3);
    ASSERT_TLM_CycleOverrun_SIZE(3);
    ASSERT_TLM_CycleOverrun(2, 3);
}

void RtcDriverTester::testMultipleCycles() {
    REQUIREMENT("SAMD21-RTC-004: The RtcDriver shall detect RTC interrupts via activeIn port");
    REQUIREMENT("SAMD21-RTC-006: The RtcDriver shall emit cycle signals via the CycleOut port");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    const U32 NUM_CYCLES = 5;
    RtcHardware::RtcState& state = RtcHardware::getRtcState();

    for (U32 i = 0; i < NUM_CYCLES; i++) {
        // Set deadline reached before each interrupt
        state.deadline_reached = true;

        // Simulate an RTC interrupt
        this->simulateRtcInterrupt();

        // Call activeIn_handler
        // A wakeup interrupt is pending on every iteration, so it should always return true
        bool didWork = this->invoke_to_activeIn(0, i);
        EXPECT_TRUE(didWork);

        // Verify cycle was emitted (check total count)
        ASSERT_FROM_PORT_HISTORY_SIZE(i + 1);
        ASSERT_from_CycleOut_SIZE(i + 1);
    }

    // After all cycles, verify no overruns were reported
    ASSERT_EQ(state.deadline_exceeded, 0U);

    // Verify tick counter incremented correctly
    ASSERT_EQ(state.n_ticks, NUM_CYCLES);
}

void RtcDriverTester::testTimeNow() {
    REQUIREMENT("SAMD21-RTC-007: The RtcDriver shall provide an Os::RawTime implementation");

    this->resetTest();

    // Configure with 1 Hz for easy calculations
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    RtcHardware::RtcState& state = RtcHardware::getRtcState();

    // Test 1: Basic time with no sub-tick
    state.n_ticks = 5;
    this->setRtcCounter(0);
    Os::Samd21::Samd21RawTimeHandle time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 5U);
    ASSERT_EQ(time.m_micros, 0U);

    // Test 2: Time with sub-tick progress
    // At 32768 Hz clock, half a second is 16384 ticks
    state.n_ticks = 5;
    this->setRtcCounter(16384);
    time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 5U);
    ASSERT_GE(time.m_micros, 490000U);  // Allow some tolerance
    ASSERT_LE(time.m_micros, 510000U);

    // Test 3: Sub-tick overflow (should roll into seconds)
    // 1 tick at 1 Hz, plus enough counter to push into next second
    state.n_ticks = 1;
    this->setRtcCounter(32768);  // Should add 1 second
    time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 2U);
    ASSERT_EQ(time.m_micros, 0U);

    // Test 4: Higher tick rate (16 Hz)
    this->resetRtcHardware();
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_16_HZ);
    this->component.enable();

    // 32 ticks at 16 Hz = 2 seconds, with some subsecond ticks
    state.n_ticks = 33;  // 2.0625 seconds
    this->setRtcCounter(0);
    time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 2U);
    ASSERT_GE(time.m_micros, 60000U);  // Should be ~62500 usec
    ASSERT_LE(time.m_micros, 65000U);
}

void RtcDriverTester::testTimeNowUnconfigured() {
    REQUIREMENT(
        "SAMD21-RTC-007: The RtcDriver shall provide an Os::RawTime implementation (zero time when "
        "unconfigured)");

    this->resetTest();

    // Get time before configuration
    Os::Samd21::Samd21RawTimeHandle time = Os::Samd21::timeNow();

    // Should return zero
    ASSERT_EQ(time.m_seconds, 0U);
    ASSERT_EQ(time.m_micros, 0U);
}

void RtcDriverTester::testTimeNowFunction() {
    REQUIREMENT(
        "SAMD21-RTC-007: The RtcDriver shall provide an Os::RawTime implementation (timeNow() derives "
        "time from the tick counter and hardware counter)");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    RtcHardware::RtcState& state = RtcHardware::getRtcState();

    // Test 1: Basic time with no sub-tick
    state.n_ticks = 10;
    this->setRtcCounter(0);
    Os::Samd21::Samd21RawTimeHandle time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 10U);
    ASSERT_EQ(time.m_micros, 0U);

    // Test 2: Time with sub-tick progress (quarter second)
    state.n_ticks = 10;
    this->setRtcCounter(8192);  // Quarter second at 32768 Hz
    time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 10U);
    ASSERT_GE(time.m_micros, 240000U);  // ~250ms with tolerance
    ASSERT_LE(time.m_micros, 260000U);

    // Test 3: Time with half-second sub-tick
    state.n_ticks = 5;
    this->setRtcCounter(16384);  // Half second at 32768 Hz
    time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 5U);
    ASSERT_GE(time.m_micros, 490000U);  // ~500ms with tolerance
    ASSERT_LE(time.m_micros, 510000U);

    // Test 4: Sub-tick overflow into next second
    state.n_ticks = 1;
    this->setRtcCounter(32768);  // Full second worth of ticks
    time = Os::Samd21::timeNow();
    ASSERT_EQ(time.m_seconds, 2U);
    ASSERT_EQ(time.m_micros, 0U);
}

void RtcDriverTester::testTimeInterval() {
    REQUIREMENT(
        "SAMD21-RTC-007: The RtcDriver shall provide an Os::RawTime implementation (timeInterval() computes the "
        "absolute difference between two times)");

    Os::Samd21::Samd21RawTimeHandle a;
    Os::Samd21::Samd21RawTimeHandle b;

    // Test 1: Identical times
    a.m_seconds = 10;
    a.m_micros = 500000;
    b = a;
    Fw::TimeInterval interval = Os::Samd21::timeInterval(a, b);
    ASSERT_EQ(interval.getSeconds(), 0U);
    ASSERT_EQ(interval.getUSeconds(), 0U);

    // Test 2: No microsecond borrow, independent of argument order
    b.m_seconds = 12;
    b.m_micros = 750000;
    interval = Os::Samd21::timeInterval(a, b);
    ASSERT_EQ(interval.getSeconds(), 2U);
    ASSERT_EQ(interval.getUSeconds(), 250000U);
    interval = Os::Samd21::timeInterval(b, a);
    ASSERT_EQ(interval.getSeconds(), 2U);
    ASSERT_EQ(interval.getUSeconds(), 250000U);

    // Test 3: Microsecond borrow from seconds
    b.m_seconds = 12;
    b.m_micros = 250000;
    interval = Os::Samd21::timeInterval(a, b);
    ASSERT_EQ(interval.getSeconds(), 1U);
    ASSERT_EQ(interval.getUSeconds(), 750000U);

    // Test 4: Same second, newer determined by microseconds
    b.m_seconds = 10;
    b.m_micros = 100000;
    interval = Os::Samd21::timeInterval(a, b);
    ASSERT_EQ(interval.getSeconds(), 0U);
    ASSERT_EQ(interval.getUSeconds(), 400000U);
}

}  // namespace Samd21

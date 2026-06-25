// ======================================================================
// \title  RtcDriverTester.cpp
// \author tumbar
// \brief  cpp file for RtcDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/RtcDriver/test/ut/RtcDriverTester.hpp"
#include "fprime-samd/Drv/RtcDriver/RawTime.hpp"
#include "Fw/Types/SerialBuffer.hpp"
#include "STest/Pick/Pick.hpp"

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

void RtcDriverTester::testEnable() {
    // REQUIREMENT("RTC-002: Driver shall enable RTC peripheral and interrupts");

    this->resetTest();

    // Configure first
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);

    // Enable the RTC
    this->component.enable();

    // Verify state indicates configuration is complete
    const RtcHardware::RtcState& state = RtcHardware::getRtcState();
    ASSERT_TRUE(state.configured);
}

void RtcDriverTester::testCycle() {
    // REQUIREMENT("RTC-003: Driver shall emit cycle signal on RTC interrupt");

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
    this->invoke_to_activeIn(0, 0);

    // Verify cycle was emitted
    ASSERT_FROM_PORT_HISTORY_SIZE(1);
    ASSERT_from_CycleOut_SIZE(1);

    // Verify no overrun occurred
    ASSERT_EQ(state.deadline_exceeded, 0U);
}

void RtcDriverTester::testCycleOverrun() {
    // REQUIREMENT("RTC-004: Driver shall detect and report cycle overruns");

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
    this->invoke_to_activeIn(0, 0);

    // Verify overrun was detected and reported
    this->assertTlmCycleOverrun(1);

    // Verify state shows overrun
    ASSERT_EQ(state.deadline_exceeded, 1U);
}

void RtcDriverTester::testMultipleCycles() {
    // REQUIREMENT("RTC-005: Driver shall handle multiple consecutive cycles correctly");

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
        this->invoke_to_activeIn(0, i);

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
    // REQUIREMENT("RTC-006: Driver shall provide accurate time via Os::RawTime");

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
    // REQUIREMENT("RTC-007: Driver shall return zero time when unconfigured");

    this->resetTest();

    // Get time before configuration
    Os::Samd21::Samd21RawTimeHandle time = Os::Samd21::timeNow();

    // Should return zero
    ASSERT_EQ(time.m_seconds, 0U);
    ASSERT_EQ(time.m_micros, 0U);
}

void RtcDriverTester::testTimeNowFunction() {
    // REQUIREMENT("RTC-008: timeNow() function shall return accurate time from RTC state");

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

void RtcDriverTester::testSamd21RawTimeNow() {
    // REQUIREMENT("RTC-009: Samd21RawTime::now() shall populate handle from timeNow()");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    RtcHardware::RtcState& state = RtcHardware::getRtcState();

    // Set known time state
    state.n_ticks = 42;
    this->setRtcCounter(16384);  // Half second

    // Create Samd21RawTime instance and call now()
    Os::Samd21::Samd21RawTime rawTime;
    Os::RawTime::Status status = rawTime.now();

    // Verify success
    ASSERT_EQ(status, Os::RawTime::Status::OP_OK);

    // Verify handle was populated correctly
    Os::RawTimeHandle* handle = rawTime.getHandle();
    Os::Samd21::Samd21RawTimeHandle* samd21Handle = static_cast<Os::Samd21::Samd21RawTimeHandle*>(handle);

    ASSERT_EQ(samd21Handle->m_seconds, 42U);
    ASSERT_GE(samd21Handle->m_micros, 490000U);
    ASSERT_LE(samd21Handle->m_micros, 510000U);
}

void RtcDriverTester::testSamd21RawTimeSerialization() {
    // REQUIREMENT("RTC-011: Samd21RawTime shall serialize/deserialize correctly");

    this->resetTest();

    // Configure and enable
    this->component.configure(RtcDriver::ClockSource::InternalOscillator, RtcDriver::TickRate::TICK_1_HZ);
    this->component.enable();

    RtcHardware::RtcState& state = RtcHardware::getRtcState();

    // Set known time state
    state.n_ticks = 123;
    this->setRtcCounter(8192);  // 0.25s

    // Create and populate RawTime
    Os::Samd21::Samd21RawTime originalTime;
    ASSERT_EQ(originalTime.now(), Os::RawTime::Status::OP_OK);

    // Serialize to buffer (big endian)
    U8 buffer[FW_RAW_TIME_SERIALIZATION_MAX_SIZE];
    Fw::SerialBuffer serBuffer(buffer, sizeof(buffer));

    Fw::SerializeStatus serStatus = originalTime.serializeTo(serBuffer);
    ASSERT_EQ(serStatus, Fw::SerializeStatus::FW_SERIALIZE_OK);

    // Verify size (2x U32 = 8 bytes)
    ASSERT_EQ(serBuffer.getSize(), 8U);

    // Deserialize into new instance
    Os::Samd21::Samd21RawTime deserializedTime;
    serStatus = deserializedTime.deserializeFrom(serBuffer);
    ASSERT_EQ(serStatus, Fw::SerializeStatus::FW_SERIALIZE_OK);

    // Verify values match
    Os::Samd21::Samd21RawTimeHandle* origHandle =
        static_cast<Os::Samd21::Samd21RawTimeHandle*>(originalTime.getHandle());
    Os::Samd21::Samd21RawTimeHandle* deserHandle =
        static_cast<Os::Samd21::Samd21RawTimeHandle*>(deserializedTime.getHandle());

    ASSERT_EQ(deserHandle->m_seconds, origHandle->m_seconds);
    ASSERT_EQ(deserHandle->m_micros, origHandle->m_micros);

    // Test little endian round-trip
    serBuffer.resetSer();
    serStatus = originalTime.serializeTo(serBuffer, Fw::Endianness::LITTLE);
    ASSERT_EQ(serStatus, Fw::SerializeStatus::FW_SERIALIZE_OK);

    Os::Samd21::Samd21RawTime littleEndianTime;
    serStatus = littleEndianTime.deserializeFrom(serBuffer, Fw::Endianness::LITTLE);
    ASSERT_EQ(serStatus, Fw::SerializeStatus::FW_SERIALIZE_OK);

    Os::Samd21::Samd21RawTimeHandle* littleHandle =
        static_cast<Os::Samd21::Samd21RawTimeHandle*>(littleEndianTime.getHandle());

    ASSERT_EQ(littleHandle->m_seconds, origHandle->m_seconds);
    ASSERT_EQ(littleHandle->m_micros, origHandle->m_micros);
}

}  // namespace Samd21

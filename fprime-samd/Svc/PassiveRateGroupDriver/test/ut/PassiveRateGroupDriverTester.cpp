// ======================================================================
// \title  PassiveRateGroupDriverTester.cpp
// \author tumbar
// \brief  cpp file for PassiveRateGroupDriver component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveRateGroupDriver/test/ut/PassiveRateGroupDriverTester.hpp"
#include <unistd.h>
#include <cstring>
#include <limits>
#include "Fw/Test/UnitTest.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PassiveRateGroupDriverTester::PassiveRateGroupDriverTester()
    : PassiveRateGroupDriverGTestBase("PassiveRateGroupDriverTester", PassiveRateGroupDriverTester::MAX_HISTORY_SIZE),
      component("PassiveRateGroupDriver"),
      m_tick(0),
      m_fired(),
      m_cycleStart(),
      m_timestampsOrdered(true),
      m_stallUs(0) {
    this->initComponents();
    this->connectPorts();
}

PassiveRateGroupDriverTester::~PassiveRateGroupDriverTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PassiveRateGroupDriverTester::testDeploymentDividers() {
    REQUIREMENT("SAMD21-RGDRIVER-001");
    // The FFB / Breadboard table: 8 Hz, 1 Hz and 0.1 Hz off an 8 Hz tick
    PassiveRateGroupDriver::DividerSet set{{{1, 0}, {8, 0}, {80, 0}}};
    this->component.configure(set);
    this->tick(160);
    this->assertSchedule(0, 1, 0);
    this->assertSchedule(1, 8, 0);
    this->assertSchedule(2, 80, 0);
    ASSERT_from_CycleOut_SIZE(160 + 20 + 2);
}

void PassiveRateGroupDriverTester::testOffsets() {
    REQUIREMENT("SAMD21-RGDRIVER-001");
    PassiveRateGroupDriver::DividerSet set{{{4, 1}, {3, 2}, {2, 0}}};
    this->component.configure(set);
    this->tick(48);
    this->assertSchedule(0, 4, 1);
    this->assertSchedule(1, 3, 2);
    this->assertSchedule(2, 2, 0);
}

void PassiveRateGroupDriverTester::testZeroDivisorDisabled() {
    REQUIREMENT("SAMD21-RGDRIVER-002");
    PassiveRateGroupDriver::DividerSet set{{{0, 0}, {2, 1}, {0, 0}}};
    this->component.configure(set);
    this->tick(20);
    for (FwSizeType t = 0; t < this->m_tick; t++) {
        ASSERT_FALSE(this->m_fired[0][t]) << "tick " << t;
        ASSERT_FALSE(this->m_fired[2][t]) << "tick " << t;
    }
    this->assertSchedule(1, 2, 1);
}

void PassiveRateGroupDriverTester::testUnconnectedPortSkipped() {
    REQUIREMENT("SAMD21-RGDRIVER-002");
    // A second instance with only CycleOut[0] and CycleOut[2] connected
    PassiveRateGroupDriver partial("Partial");
    partial.init(TEST_INSTANCE_ID);
    partial.set_CycleOut_OutputPort(0, this->get_from_CycleOut(0));
    partial.set_CycleOut_OutputPort(2, this->get_from_CycleOut(2));
    PassiveRateGroupDriver::DividerSet set{{{1, 0}, {1, 0}, {2, 0}}};
    partial.configure(set);

    for (FwSizeType t = 0; t < 10; t++) {
        this->m_cycleStart.now();
        partial.get_CycleIn_InputPort(0)->invoke(this->m_cycleStart);
        this->m_tick++;
    }
    for (FwSizeType t = 0; t < this->m_tick; t++) {
        ASSERT_FALSE(this->m_fired[1][t]) << "tick " << t;
    }
    this->assertSchedule(0, 1, 0);
    this->assertSchedule(2, 2, 0);
}

void PassiveRateGroupDriverTester::testRollover() {
    REQUIREMENT("SAMD21-RGDRIVER-003");
    // Rollover is lcm(3, 4, 5) = 60; run through it three times to cross the wrap twice
    PassiveRateGroupDriver::DividerSet set{{{3, 0}, {4, 1}, {5, 4}}};
    this->component.configure(set);
    this->tick(180);
    this->assertSchedule(0, 3, 0);
    this->assertSchedule(1, 4, 1);
    this->assertSchedule(2, 5, 4);
}

void PassiveRateGroupDriverTester::testReconfigure() {
    REQUIREMENT("SAMD21-RGDRIVER-001");
    PassiveRateGroupDriver::DividerSet first{{{2, 0}, {3, 0}, {5, 0}}};
    this->component.configure(first);
    this->tick(7);
    this->assertSchedule(0, 2, 0);
    this->assertSchedule(1, 3, 0);
    this->assertSchedule(2, 5, 0);

    // Re-configuring restarts the schedule from tick 0 with the new table, so port 1 fires on the
    // first tick after it even though 7 ticks ran under the first table.
    const FwSizeType big = static_cast<FwSizeType>(1) << (std::numeric_limits<FwSizeType>::digits / 3);
    PassiveRateGroupDriver::DividerSet second{{{1, 0}, {big, 0}, {big, 1}}};
    this->component.configure(second);
    this->component.configure(second);
    this->resetTest();
    this->tick(4);
    this->assertSchedule(0, 1, 0);
    this->assertSchedule(1, big, 0);
    this->assertSchedule(2, big, 1);
}

void PassiveRateGroupDriverTester::testCycleStartTimestamp() {
    REQUIREMENT("SAMD21-RGDRIVER-004");
    PassiveRateGroupDriver::DividerSet set{{{1, 0}, {1, 0}, {1, 0}}};
    this->component.configure(set);
    this->tick(5);
    ASSERT_from_CycleOut_SIZE(15);
    ASSERT_TRUE(this->m_timestampsOrdered);
}

void PassiveRateGroupDriverTester::testCycleTimeTelemetry() {
    REQUIREMENT("SAMD21-RGDRIVER-005");
    const U32 stall = 20000;
    PassiveRateGroupDriver::DividerSet set{{{1, 0}, {0, 0}, {0, 0}}};
    this->component.configure(set);

    // A long cycle sets the maximum
    this->m_stallUs = stall;
    this->tick(1);
    ASSERT_TLM_SIZE(2);
    ASSERT_TLM_CycleTime_SIZE(1);
    ASSERT_TLM_MaxCycleTime_SIZE(1);
    const U32 longCycle = this->tlmHistory_CycleTime->at(0).arg;
    ASSERT_GE(longCycle, stall);
    ASSERT_TLM_MaxCycleTime(0, longCycle);

    // A short cycle reports its own time and keeps the maximum
    this->m_stallUs = 0;
    this->tick(1);
    ASSERT_TLM_CycleTime_SIZE(2);
    ASSERT_TLM_MaxCycleTime_SIZE(2);
    ASSERT_LT(this->tlmHistory_CycleTime->at(1).arg, stall);
    ASSERT_TLM_MaxCycleTime(1, longCycle);

    // Both channels are written on every tick, even when no port fires
    PassiveRateGroupDriver::DividerSet idle{{{0, 0}, {0, 0}, {0, 0}}};
    this->component.configure(idle);
    this->tick(3);
    ASSERT_from_CycleOut_SIZE(2);
    ASSERT_TLM_CycleTime_SIZE(5);
    ASSERT_TLM_MaxCycleTime_SIZE(5);
    ASSERT_TLM_MaxCycleTime(4, longCycle);
}

void PassiveRateGroupDriverTester::testNotConfiguredAsserts() {
    REQUIREMENT("SAMD21-RGDRIVER-006");
    ASSERT_DEATH_IF_SUPPORTED(this->tick(1), "Assert:");
}

void PassiveRateGroupDriverTester::testOffsetNotBelowDivisorAsserts() {
    REQUIREMENT("SAMD21-RGDRIVER-006");
    PassiveRateGroupDriver::DividerSet equal{{{1, 0}, {4, 4}, {1, 0}}};
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(equal), "Assert:");
    PassiveRateGroupDriver::DividerSet above{{{1, 0}, {1, 0}, {2, 3}}};
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(above), "Assert:");
    // A disabled port may not carry an offset either
    PassiveRateGroupDriver::DividerSet disabled{{{0, 1}, {1, 0}, {1, 0}}};
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(disabled), "Assert:");
}

void PassiveRateGroupDriverTester::testRolloverOverflowAsserts() {
    REQUIREMENT("SAMD21-RGDRIVER-006");
    // max / 2 is odd, so its least common multiple with 4 is 4 * (max / 2), which overflows
    const FwSizeType half = std::numeric_limits<FwSizeType>::max() / 2;
    PassiveRateGroupDriver::DividerSet set{{{half, 0}, {4, 0}, {1, 0}}};
    ASSERT_DEATH_IF_SUPPORTED(this->component.configure(set), "Assert:");
}

void PassiveRateGroupDriverTester::testLargeCommonDivisors() {
    REQUIREMENT("SAMD21-RGDRIVER-003");
    REQUIREMENT("SAMD21-RGDRIVER-006");
    // Divisors whose product overflows FwSizeType but whose least common multiple does not
    // are accepted, and the schedule is still exact
    const FwSizeType big = static_cast<FwSizeType>(1) << (std::numeric_limits<FwSizeType>::digits / 2);
    PassiveRateGroupDriver::DividerSet set{{{big, 0}, {big, 2}, {big / 2, 1}}};
    this->component.configure(set);
    this->tick(5);
    this->assertSchedule(0, big, 0);
    this->assertSchedule(1, big, 2);
    this->assertSchedule(2, big / 2, 1);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void PassiveRateGroupDriverTester::from_CycleOut_handler(FwIndexType portNum, Os::RawTime& cycleStart) {
    this->pushFromPortEntry_CycleOut(cycleStart);
    FW_ASSERT(portNum >= 0 && portNum < NUM_PORTS, static_cast<FwAssertArgType>(portNum));
    FW_ASSERT(this->m_tick < MAX_TICKS, static_cast<FwAssertArgType>(this->m_tick));
    this->m_fired[portNum][this->m_tick] = true;

    // The port's timestamp must be taken no earlier than the CycleIn start: measured against the
    // same later instant, the port's timestamp is never further away than the cycle start
    Os::RawTime now;
    now.now();
    U32 portToNow = 0;
    U32 startToNow = 0;
    (void)now.getDiffUsec(cycleStart, portToNow);
    (void)now.getDiffUsec(this->m_cycleStart, startToNow);
    if (portToNow > startToNow) {
        this->m_timestampsOrdered = false;
    }

    if (this->m_stallUs > 0) {
        (void)usleep(this->m_stallUs);
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void PassiveRateGroupDriverTester::resetTest() {
    this->clearHistory();
    this->m_tick = 0;
    (void)std::memset(this->m_fired, 0, sizeof(this->m_fired));
    this->m_timestampsOrdered = true;
}

void PassiveRateGroupDriverTester::tick(FwSizeType count) {
    for (FwSizeType i = 0; i < count; i++) {
        this->m_cycleStart.now();
        this->invoke_to_CycleIn(0, this->m_cycleStart);
        this->m_tick++;
    }
}

void PassiveRateGroupDriverTester::assertSchedule(FwIndexType port, FwSizeType divisor, FwSizeType offset) const {
    for (FwSizeType t = 0; t < this->m_tick; t++) {
        const bool expected = (t % divisor) == offset;
        ASSERT_EQ(this->m_fired[port][t], expected) << "port " << port << " tick " << t;
    }
}

}  // namespace Samd21

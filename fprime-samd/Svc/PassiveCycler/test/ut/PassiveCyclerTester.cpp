// ======================================================================
// \title  PassiveCyclerTester.cpp
// \author tumbar
// \brief  cpp file for PassiveCycler component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveCycler/test/ut/PassiveCyclerTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"

namespace Svc {

constexpr FwIndexType PassiveCyclerTester::NUM_PORTS;
constexpr U32 PassiveCyclerTester::MAX_WORK;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PassiveCyclerTester::PassiveCyclerTester()
    : PassiveCyclerGTestBase("PassiveCyclerTester", PassiveCyclerTester::MAX_HISTORY_SIZE),
      component("PassiveCycler"),
      m_partial("PassiveCyclerPartial"),
      m_work(),
      m_remaining() {
    this->initComponents();
    this->connectPorts();
    this->m_partial.init(TEST_INSTANCE_ID);
}

PassiveCyclerTester::~PassiveCyclerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PassiveCyclerTester::testIdle() {
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-001: cycle() shall call each connected cycleOut port once per pass, in ascending "
        "port order, passing the port index as the context");
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-002: cycle() shall repeat passes until a pass in which no consumer returns true, "
        "then return");

    bool connected[NUM_PORTS];
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        connected[i] = true;
    }
    const U32 work[NUM_PORTS] = {};
    this->setWork(work);
    this->cycle(this->component);
    this->assertPasses(connected);
}

void PassiveCyclerTester::testRepeatUntilIdle() {
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-002: cycle() shall repeat passes until a pass in which no consumer returns true, "
        "then return");
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-003: Every connected consumer shall be called on every pass, including after an "
        "earlier consumer in the same pass returned true");

    bool connected[NUM_PORTS];
    U32 work[NUM_PORTS] = {};
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        connected[i] = true;
    }
    // The first port has work on three passes, so every port runs on four passes
    work[0] = 3;
    this->setWork(work);
    this->cycle(this->component);
    this->assertPasses(connected);
    ASSERT_from_cycleOut_SIZE(4 * NUM_PORTS);
}

void PassiveCyclerTester::testLastPortWorks() {
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-003: Every connected consumer shall be called on every pass, including after an "
        "earlier consumer in the same pass returned true");

    bool connected[NUM_PORTS];
    U32 work[NUM_PORTS] = {};
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        connected[i] = true;
    }
    work[NUM_PORTS - 1] = 1;
    this->setWork(work);
    this->cycle(this->component);
    this->assertPasses(connected);
    ASSERT_from_cycleOut_SIZE(2 * NUM_PORTS);
}

void PassiveCyclerTester::testRandomWork() {
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-001: cycle() shall call each connected cycleOut port once per pass, in ascending "
        "port order, passing the port index as the context");
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-002: cycle() shall repeat passes until a pass in which no consumer returns true, "
        "then return");
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-003: Every connected consumer shall be called on every pass, including after an "
        "earlier consumer in the same pass returned true");

    bool connected[NUM_PORTS];
    U32 work[NUM_PORTS];
    for (U32 iteration = 0; iteration < 100; iteration++) {
        for (FwIndexType i = 0; i < NUM_PORTS; i++) {
            connected[i] = true;
            work[i] = STest::Pick::lowerUpper(0, MAX_WORK);
        }
        this->setWork(work);
        this->cycle(this->component);
        this->assertPasses(connected);
    }
}

void PassiveCyclerTester::testUnconnectedPorts() {
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-001: cycle() shall call each connected cycleOut port once per pass, in ascending "
        "port order, passing the port index as the context");
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-004: cycle() shall skip unconnected cycleOut ports, and shall return without "
        "calling anything when no port is connected");

    bool connected[NUM_PORTS];
    U32 work[NUM_PORTS];
    for (U32 iteration = 0; iteration < 100; iteration++) {
        for (FwIndexType i = 0; i < NUM_PORTS; i++) {
            connected[i] = (STest::Pick::lowerUpper(0, 1) == 1);
            work[i] = STest::Pick::lowerUpper(0, MAX_WORK);
        }
        // Make sure the first and last ports are covered both ways across iterations
        connected[0] = (iteration % 2) == 0;
        connected[NUM_PORTS - 1] = (iteration % 2) == 1;

        // A fresh component per iteration: output ports cannot be disconnected
        PassiveCycler partial("PassiveCyclerPartial");
        partial.init(TEST_INSTANCE_ID);
        for (FwIndexType i = 0; i < NUM_PORTS; i++) {
            if (connected[i]) {
                partial.set_cycleOut_OutputPort(i, this->get_from_cycleOut(i));
            }
        }
        this->setWork(work);
        this->cycle(partial);
        this->assertPasses(connected);
    }
}

void PassiveCyclerTester::testNoPortsConnected() {
    REQUIREMENT(
        "SAMD21-PASSIVECYCLER-004: cycle() shall skip unconnected cycleOut ports, and shall return without "
        "calling anything when no port is connected");

    U32 work[NUM_PORTS];
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        work[i] = MAX_WORK;
    }
    this->setWork(work);
    this->cycle(this->m_partial);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    // No consumer ran, so none of the work was taken
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        ASSERT_EQ(this->m_remaining[i], MAX_WORK);
    }
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

bool PassiveCyclerTester::from_cycleOut_handler(FwIndexType portNum, U32 context) {
    this->pushFromPortEntry_cycleOut(context);
    // Record the port that was called so the order can be checked; context should equal it
    EXPECT_EQ(context, static_cast<U32>(portNum));
    if (this->m_remaining[portNum] > 0) {
        this->m_remaining[portNum]--;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void PassiveCyclerTester::setWork(const U32 work[NUM_PORTS]) {
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        FW_ASSERT(work[i] <= MAX_WORK, static_cast<FwAssertArgType>(work[i]));
        this->m_work[i] = work[i];
        this->m_remaining[i] = work[i];
    }
}

void PassiveCyclerTester::cycle(PassiveCycler& cycler) {
    this->clearHistory();
    cycler.cycle();
}

void PassiveCyclerTester::assertPasses(const bool connected[NUM_PORTS]) {
    U32 maxWork = 0;
    FwSizeType connectedCount = 0;
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        if (connected[i]) {
            connectedCount++;
            maxWork = (this->m_work[i] > maxWork) ? this->m_work[i] : maxWork;
        }
    }
    const FwSizeType passes = (connectedCount == 0) ? 0 : static_cast<FwSizeType>(maxWork) + 1;
    ASSERT_FROM_PORT_HISTORY_SIZE(passes * connectedCount);
    ASSERT_from_cycleOut_SIZE(passes * connectedCount);

    // Each pass calls every connected port once, lowest index first
    FwSizeType entry = 0;
    for (FwSizeType pass = 0; pass < passes; pass++) {
        for (FwIndexType i = 0; i < NUM_PORTS; i++) {
            if (connected[i]) {
                ASSERT_from_cycleOut(entry, static_cast<U32>(i));
                entry++;
            }
        }
    }

    // Every connected port used up all its work
    for (FwIndexType i = 0; i < NUM_PORTS; i++) {
        if (connected[i]) {
            ASSERT_EQ(this->m_remaining[i], 0U);
        }
    }
}

}  // namespace Svc

// ======================================================================
// \title  PassiveCyclerTester.cpp
// \brief  cpp file for PassiveCycler component test harness implementation class
// ======================================================================

#include "PassiveCyclerTester.hpp"
#include "fprime-samd/Drv/Types/Idle.hpp"

namespace Svc {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PassiveCyclerTester ::PassiveCyclerTester()
    : PassiveCyclerGTestBase("PassiveCyclerTester", PassiveCyclerTester::MAX_HISTORY_SIZE),
      component("PassiveCycler"),
      m_sequenceSize(0),
      m_interruptAt(-1) {
    for (FwIndexType i = 0; i < NUM_CONSUMERS; i++) {
        this->m_calls[i] = 0;
        this->m_workRemaining[i] = 0;
    }
    Samd21::IdleStub::resetState();
    this->initComponents();
    this->connectPorts();
}

PassiveCyclerTester ::~PassiveCyclerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PassiveCyclerTester ::testIdle() {
    // REQUIREMENT(SAMD21-CYC-001, SAMD21-CYC-002): one pass over every consumer, then stop
    this->assertPasses(1, false);
}

void PassiveCyclerTester ::testEarlyTrue() {
    // REQUIREMENT(SAMD21-CYC-001): a true from consumer 0 does not skip consumers 1..N-1 in the same pass
    // REQUIREMENT(SAMD21-CYC-002): the pass is repeated once, then everybody is idle
    this->m_workRemaining[0] = 1;
    this->assertPasses(2, false);
}

void PassiveCyclerTester ::testLateTrue() {
    // REQUIREMENT(SAMD21-CYC-002): work reported by the last consumer still triggers a full second pass
    this->m_workRemaining[NUM_CONSUMERS - 1] = 1;
    this->assertPasses(2, false);
}

void PassiveCyclerTester ::testRepeatedWork() {
    // REQUIREMENT(SAMD21-CYC-001, SAMD21-CYC-002): n trues from one consumer give n + 1 full passes
    this->m_workRemaining[1] = 3;
    this->m_workRemaining[NUM_CONSUMERS / 2] = 2;
    this->assertPasses(4, false);
}

void PassiveCyclerTester ::testIdleSleeps() {
    // REQUIREMENT(SAMD21-CYC-004): quiet pass -> WFI
    const Samd21::IdleStub::State& idle = Samd21::IdleStub::getState();
    this->assertPasses(1, true);
    ASSERT_EQ(idle.arms, 1u);
    ASSERT_EQ(idle.sleeps, 1u);
    ASSERT_EQ(idle.skips, 0u);
    // Work reported by a consumer adds passes but the turn still ends in a sleep
    this->m_workRemaining[2] = 1;
    this->assertPasses(3, true);
    ASSERT_EQ(idle.arms, 2u);
    ASSERT_EQ(idle.sleeps, 2u);
    ASSERT_EQ(idle.skips, 0u);
}

void PassiveCyclerTester ::testIdleInterruptDuringPass() {
    // REQUIREMENT(SAMD21-CYC-005): an interrupt taken during the pass (here while the last
    //                              consumer runs, after everybody who could have acted on
    //                              it) skips the sleep; the next turn services it and sleeps
    const Samd21::IdleStub::State& idle = Samd21::IdleStub::getState();
    this->m_interruptAt = NUM_CONSUMERS - 1;
    this->assertPasses(1, true);
    ASSERT_EQ(this->m_interruptAt, -1);
    ASSERT_EQ(idle.arms, 1u);
    ASSERT_EQ(idle.sleeps, 0u);
    ASSERT_EQ(idle.skips, 1u);
    this->assertPasses(2, true);
    ASSERT_EQ(idle.arms, 2u);
    ASSERT_EQ(idle.sleeps, 1u);
    ASSERT_EQ(idle.skips, 1u);
    // Same with the interrupt landing while the first consumer runs
    this->m_interruptAt = 0;
    this->assertPasses(3, true);
    ASSERT_EQ(idle.sleeps, 1u);
    ASSERT_EQ(idle.skips, 2u);
}

void PassiveCyclerTester ::testIdleStaleInterrupt() {
    // REQUIREMENT(SAMD21-CYC-004): an interrupt taken before the turn is serviced by its pass;
    //                              arm() forgets it so the turn ends in a sleep
    const Samd21::IdleStub::State& idle = Samd21::IdleStub::getState();
    Samd21::IdleStub::getState().interrupted = true;
    this->assertPasses(1, true);
    ASSERT_EQ(idle.sleeps, 1u);
    ASSERT_EQ(idle.skips, 0u);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

bool PassiveCyclerTester ::from_cycleOut_handler(FwIndexType portNum, U32 context) {
    this->pushFromPortEntry_cycleOut(context);
    this->m_calls[portNum]++;
    if (this->m_sequenceSize < MAX_HISTORY_SIZE) {
        this->m_sequence[this->m_sequenceSize] = portNum;
    }
    this->m_sequenceSize++;
    if (portNum == this->m_interruptAt) {
        Samd21::IdleStub::getState().interrupted = true;
        this->m_interruptAt = -1;
    }
    if (this->m_workRemaining[portNum] > 0) {
        this->m_workRemaining[portNum]--;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void PassiveCyclerTester ::assertPasses(U32 passes, bool viaIdle) {
    if (viaIdle) {
        this->component.idle();
    } else {
        this->component.cycle();
    }
    ASSERT_EQ(this->m_sequenceSize, static_cast<FwSizeType>(passes) * NUM_CONSUMERS);
    ASSERT_from_cycleOut_SIZE(static_cast<FwSizeType>(passes) * NUM_CONSUMERS);
    for (FwIndexType i = 0; i < NUM_CONSUMERS; i++) {
        ASSERT_EQ(this->m_calls[i], passes) << "consumer " << i;
        ASSERT_EQ(this->m_workRemaining[i], 0u) << "consumer " << i;
    }
    for (FwSizeType n = 0; n < this->m_sequenceSize; n++) {
        const FwIndexType expected = static_cast<FwIndexType>(n % NUM_CONSUMERS);
        ASSERT_EQ(this->m_sequence[n], expected) << "call " << n;
        ASSERT_from_cycleOut(n, static_cast<U32>(expected));
    }
}

}  // namespace Svc

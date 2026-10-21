// ======================================================================
// \title  CyclerSchedAdapterTester.cpp
// \author lestarch-autobot
// \brief  cpp file for CyclerSchedAdapter component test harness implementation class
// ======================================================================

#include "CyclerSchedAdapterTester.hpp"

#include "STest/Pick/Pick.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

CyclerSchedAdapterTester ::CyclerSchedAdapterTester()
    : CyclerSchedAdapterGTestBase("CyclerSchedAdapterTester", CyclerSchedAdapterTester::MAX_HISTORY_SIZE),
      component("CyclerSchedAdapter"),
      second("CyclerSchedAdapterSecond"),
      m_inActiveIn(false),
      m_handlerInsideActiveIn(false),
      m_handlerCalls(0) {
    this->initComponents();
    this->connectPorts();
    // The second instance shares the tester's single from_schedOut port (UT-005)
    this->second.init(CyclerSchedAdapterTester::TEST_INSTANCE_ID + 1);
    this->second.set_schedOut_OutputPort(0, this->get_from_schedOut(0));
}

CyclerSchedAdapterTester ::~CyclerSchedAdapterTester() {
    this->second.deinit();
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void CyclerSchedAdapterTester ::testForwardOnce() {
    this->clearHistory();
    this->assertSingleForward(this->component, STest::Pick::any());
    ASSERT_EQ(this->m_handlerCalls, 1U);
}

void CyclerSchedAdapterTester ::testContextPassThrough() {
    this->clearHistory();
    const U32 contexts[] = {0U, 1U, 7U, 0xFFFFFFFFU};
    for (const U32 context : contexts) {
        this->assertSingleForward(this->component, context);
    }
}

void CyclerSchedAdapterTester ::testNestedCompletion() {
    this->clearHistory();
    ASSERT_EQ(this->m_handlerCalls, 0U);
    this->assertSingleForward(this->component, STest::Pick::any());
    // Nothing is called once activeIn has returned: the count seen inside the window is final
    ASSERT_EQ(this->m_handlerCalls, 1U);
    ASSERT_from_schedOut_SIZE(1);
}

void CyclerSchedAdapterTester ::testUnconnected() {
    // An instance whose schedOut is left unconnected: the autocoded schedOut_out asserts
    CyclerSchedAdapter unconnected("CyclerSchedAdapterUnconnected");
    unconnected.init(CyclerSchedAdapterTester::TEST_INSTANCE_ID + 2);
    ASSERT_DEATH_IF_SUPPORTED(unconnected.get_activeIn_InputPort(0)->invoke(3U), "Assert");
    // Control: the connected instance still forwards exactly once
    this->clearHistory();
    this->assertSingleForward(this->component, 3U);
    unconnected.deinit();
}

void CyclerSchedAdapterTester ::testHistoryIndependence() {
    this->clearHistory();
    for (U32 i = 0; i < CyclerSchedAdapterTester::INTERLEAVED_CALLS; i++) {
        CyclerSchedAdapter& target = ((i % 2) == 0) ? this->component : this->second;
        this->assertSingleForward(target, STest::Pick::any());
    }
    const U32 expectedCalls = CyclerSchedAdapterTester::INTERLEAVED_CALLS;
    ASSERT_EQ(this->m_handlerCalls, expectedCalls);
}

void CyclerSchedAdapterTester ::testNoIdleOutput() {
    // Construction, init and connection happened in the constructor; nothing may have been called
    ASSERT_from_schedOut_SIZE(0);
    ASSERT_EQ(this->m_handlerCalls, 0U);
}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void CyclerSchedAdapterTester ::from_schedOut_handler(FwIndexType portNum, U32 context) {
    this->m_handlerInsideActiveIn = this->m_inActiveIn;
    this->m_handlerCalls++;
    this->pushFromPortEntry_schedOut(context);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

bool CyclerSchedAdapterTester ::invokeActiveIn(CyclerSchedAdapter& target, U32 context) {
    this->m_handlerInsideActiveIn = false;
    this->m_inActiveIn = true;
    const bool result = target.get_activeIn_InputPort(0)->invoke(context);
    this->m_inActiveIn = false;
    return result;
}

void CyclerSchedAdapterTester ::assertSingleForward(CyclerSchedAdapter& target, U32 context) {
    this->clearFromPortHistory();
    const bool result = this->invokeActiveIn(target, context);
    ASSERT_FALSE(result);
    ASSERT_from_schedOut_SIZE(1);
    ASSERT_from_schedOut(0, context);
    ASSERT_TRUE(this->m_handlerInsideActiveIn);
}

}  // namespace Samd21

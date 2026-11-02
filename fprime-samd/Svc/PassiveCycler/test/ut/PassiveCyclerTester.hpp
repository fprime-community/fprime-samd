// ======================================================================
// \title  PassiveCyclerTester.hpp
// \brief  hpp file for PassiveCycler component test harness implementation class
// ======================================================================

#ifndef Svc_PassiveCyclerTester_HPP
#define Svc_PassiveCyclerTester_HPP

#include "fprime-samd/Svc/PassiveCycler/PassiveCycler.hpp"
#include "fprime-samd/Svc/PassiveCycler/PassiveCyclerGTestBase.hpp"

namespace Svc {

class PassiveCyclerTester final : public PassiveCyclerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 100;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Number of consumers wired to cycleOut
    static const FwIndexType NUM_CONSUMERS = PassiveCycler::NUM_CYCLEOUT_OUTPUT_PORTS;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object PassiveCyclerTester
    PassiveCyclerTester();

    //! Destroy object PassiveCyclerTester
    ~PassiveCyclerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Nobody has work: one pass, every consumer called once, in index order, context = index
    void testIdle();

    //! The first consumer reports work once: the later consumers are still called in that
    //! pass, then every consumer is called again in the second pass
    void testEarlyTrue();

    //! The last consumer reports work once: two full passes
    void testLateTrue();

    //! A consumer reports work n times: n + 1 full passes, no consumer skipped in any of them
    void testRepeatedWork();

    //! idle() with no interrupt: arm, one pass, sleep
    void testIdleSleeps();

    //! idle() with an interrupt taken during the pass (after the consumer that would have
    //! handled it): no sleep; the next idle() runs the pass again and sleeps
    void testIdleInterruptDuringPass();

    //! An interrupt taken before idle() is serviced by its pass and does not prevent the sleep
    void testIdleStaleInterrupt();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Record the call and report work while the port's budget lasts
    bool from_cycleOut_handler(FwIndexType portNum, U32 context) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Run cycle() (or idle() when viaIdle) and assert exactly `passes` full passes so far:
    //! every consumer called `passes` times, in index order, with context equal to its index
    void assertPasses(U32 passes, bool viaIdle);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    PassiveCycler component;

    //! Calls received per consumer
    U32 m_calls[NUM_CONSUMERS];

    //! Remaining calls on which the consumer answers true
    U32 m_workRemaining[NUM_CONSUMERS];

    //! Port index of every call, in order
    FwIndexType m_sequence[MAX_HISTORY_SIZE];

    //! Number of calls recorded in m_sequence
    FwSizeType m_sequenceSize;

    //! Consumer whose next call simulates an ISR landing while it runs (-1: none)
    FwIndexType m_interruptAt;
};

}  // namespace Svc

#endif

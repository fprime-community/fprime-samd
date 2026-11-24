// ======================================================================
// \title  PassiveCyclerTester.hpp
// \author tumbar
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

    //! Number of cycleOut ports
    static constexpr FwIndexType NUM_PORTS = PassiveCyclerComponentBase::NUM_CYCLEOUT_OUTPUT_PORTS;

    //! Most passes any test asks a port to report work for
    static constexpr U32 MAX_WORK = 16;

    // Maximum size of histories storing events, telemetry, and port outputs
    static constexpr FwSizeType MAX_HISTORY_SIZE = NUM_PORTS * (MAX_WORK + 1);

    // Instance ID supplied to the component instance under test
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    PassiveCyclerTester();
    ~PassiveCyclerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Idle consumers are each called once, in port order, with their port index
    void testIdle();

    //! A consumer reporting work causes another pass of every consumer
    void testRepeatUntilIdle();

    //! Work reported by the last port still causes earlier ports to run again
    void testLastPortWorks();

    //! Random per-port work counts
    void testRandomWork();

    //! Unconnected ports are skipped; connected ones keep their index as context
    void testUnconnectedPorts();

    //! No connected ports: cycle returns without calling anything
    void testNoPortsConnected();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Record the call and report work while the port has passes left
    bool from_cycleOut_handler(FwIndexType portNum, U32 context) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Clear history and run cycle() on the given component
    void cycle(PassiveCycler& cycler);

    //! Check the call history matches passes of every connected port in order,
    //! where passes = 1 + the most work any connected port reported
    void assertPasses(const bool connected[NUM_PORTS]);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test, with every port connected
    PassiveCycler component;

    //! Spare component with no ports connected
    PassiveCycler m_partial;

    //! Passes on which each port reports work (as loaded before the cycle)
    U32 m_work[NUM_PORTS];

    //! Passes remaining on which each port reports work
    U32 m_remaining[NUM_PORTS];

    //! Set the work each port reports
    void setWork(const U32 work[NUM_PORTS]);
};

}  // namespace Svc

#endif

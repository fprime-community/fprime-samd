// ======================================================================
// \title  PassiveRateGroupDriverTester.hpp
// \author tumbar
// \brief  hpp file for PassiveRateGroupDriver component test harness implementation class
// ======================================================================

#ifndef Samd21_PassiveRateGroupDriverTester_HPP
#define Samd21_PassiveRateGroupDriverTester_HPP

#include "fprime-samd/Svc/PassiveRateGroupDriver/PassiveRateGroupDriver.hpp"
#include "fprime-samd/Svc/PassiveRateGroupDriver/PassiveRateGroupDriverGTestBase.hpp"

namespace Samd21 {

class PassiveRateGroupDriverTester final : public PassiveRateGroupDriverGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static constexpr FwSizeType MAX_HISTORY_SIZE = 256;

    // Instance ID supplied to the component instance under test
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Number of CycleIn ticks a log can hold
    static constexpr FwSizeType MAX_TICKS = 256;

    //! Number of CycleOut ports (the tests are written for the default three)
    static constexpr FwIndexType NUM_PORTS = PassiveRateGroupDriver::DIVIDER_SIZE;
    static_assert(NUM_PORTS == 3, "Tests assume three CycleOut ports");

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    PassiveRateGroupDriverTester();
    ~PassiveRateGroupDriverTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testDeploymentDividers();
    void testOffsets();
    void testZeroDivisorDisabled();
    void testUnconnectedPortSkipped();
    void testRollover();
    void testReconfigure();
    void testCycleStartTimestamp();
    void testCycleTimeTelemetry();
    void testNotConfiguredAsserts();
    void testOffsetNotBelowDivisorAsserts();
    void testRolloverOverflowAsserts();

    //! Large divisors with a small least common multiple are accepted
    void testLargeCommonDivisors();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Log which port fired on which tick, and optionally stall to stretch the cycle
    void from_CycleOut_handler(FwIndexType portNum, Os::RawTime& cycleStart) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Clear histories and the per-port log
    void resetTest();

    //! Invoke CycleIn `count` times, each with a fresh cycle start time
    void tick(FwSizeType count);

    //! Assert that across the logged ticks, `port` fired exactly when (tick % divisor) == offset
    void assertSchedule(FwIndexType port, FwSizeType divisor, FwSizeType offset) const;

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    PassiveRateGroupDriver component;

    //! Number of CycleIn calls since the last reset
    FwSizeType m_tick;

    //! m_fired[port][tick] is true when the port was called on that tick
    bool m_fired[NUM_PORTS][MAX_TICKS];

    //! Cycle start passed in to the current CycleIn
    Os::RawTime m_cycleStart;

    //! True when every CycleOut timestamp was taken at or after its CycleIn start
    bool m_timestampsOrdered;

    //! Microseconds the CycleOut handler stalls for
    U32 m_stallUs;
};

}  // namespace Samd21

#endif

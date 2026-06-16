// ======================================================================
// \title  RtcDriverTester.hpp
// \author tumbar
// \brief  hpp file for RtcDriver test harness implementation class
// ======================================================================

#ifndef Samd21_RtcDriverTester_HPP
#define Samd21_RtcDriverTester_HPP

#include "Fw/Types/BasicTypes.hpp"
#include "fprime-samd/Drv/RtcDriver/RtcDriver.hpp"
#include "fprime-samd/Drv/RtcDriver/RtcDriverGTestBase.hpp"
#include "fprime-samd/Drv/RtcDriver/RtcDriverHardware.hpp"

namespace Samd21 {

class RtcDriverTester : public RtcDriverGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 10;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Construction and destruction
    RtcDriverTester();
    ~RtcDriverTester();

    // Tests
    void testConfigure();
    void testEnable();
    void testCycle();
    void testCycleOverrun();
    void testMultipleCycles();
    void testTimeNow();
    void testTimeNowUnconfigured();

    // RawTime function tests
    void testTimeNowFunction();
    void testSamd21RawTimeNow();
    void testSamd21RawTimeSerialization();

  private:
    //! Component under test
    RtcDriver component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Invoke CycleOut port and verify it was called
    //! \param port_num Port number
    void assertCycleOut(FwIndexType port_num);

    //! Check telemetry was updated with expected value
    //! \param expected_overrun Expected overrun count
    void assertTlmCycleOverrun(U16 expected_overrun);

    //! Reset test state between tests
    void resetTest();

    //! Test helper: simulate RTC interrupt
    void simulateRtcInterrupt();

    //! Test helper: reset RTC hardware state
    void resetRtcHardware();

    //! Test helper: set RTC counter value
    void setRtcCounter(U32 counter);
};

}  // namespace Samd21

#endif

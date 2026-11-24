// ======================================================================
// \title  FatalHandlerTester.hpp
// \author tumbar
// \brief  hpp file for FatalHandler test harness implementation class
// ======================================================================

#ifndef Samd21_FatalHandlerTester_HPP
#define Samd21_FatalHandlerTester_HPP

#include "Fw/Time/TimePortAc.hpp"
#include "fprime-samd/Svc/FatalHandler/FatalHandler.hpp"
#include "fprime-samd/Svc/FatalHandler/FatalHandlerGTestBase.hpp"
#include "fprime-samd/Svc/FatalHandler/FatalHandlerHardware.hpp"

namespace Samd21 {

class FatalHandlerTester : public FatalHandlerGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 32;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Construction and destruction
    FatalHandlerTester();
    ~FatalHandlerTester();

    // Tests
    void testNoDynamicHookRegistered();
    void testReportSendsAssertionPacket();
    void testReportUsesIdBase();
    void testReportedArgumentCounts();
    void testTimeReadOnce();
    void testLatestInstanceReports();
    void testNoInstanceResetsSilently();
    void testRecursiveAssertResets();
    void testReportWithoutFatalOut();
    void testTerminateResets();
    void testFwAssertRoutedToHandler();
    void testFatalReceiveAsserts();

  private:
    //! Component under test
    FatalHandler component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Reset test and stub hardware state between test actions
    void resetTest();

    //! Arguments of one assertion as passed to the assert hook
    struct AssertArgs {
        U32 lineNo;
        FwSizeType numArgs;
        FwAssertArgType args[6];
    };

    //! Pick a random time and make it the tester's timeGet result
    //! \return The picked time
    Fw::Time pickTime();

    //! Pick a random line number and numArgs random arguments
    static AssertArgs pickArgs(FwSizeType numArgs);

    //! Drive the link-time assert hook the way Fw::defaultSwAssert does without a
    //! registered Fw::AssertHook: format into an FW_ASSERT_TEXT_SIZE buffer, then print.
    //! terminateOnAssert() is left to the tests that check it.
    static void invokeReport(const AssertArgs& args);

    //! Assert that fatalOut carried exactly PACKET_REPEAT identical copies of the
    //! AssertionFailed event packet describing args, with the given event ID and time.
    //! Each copy is decoded as an Fw::LogPacket and dispatched as an event.
    void assertPacketsSent(const AssertArgs& args, FwEventIdType id, const Fw::Time& time);

    //! Time source that returns a different time on every call
    static void countingTime(Fw::PassiveComponentBase* callComp, FwIndexType portNum, Fw::Time& time);

    //! Port that countingTime() is attached to
    Fw::InputTimePort m_countingTimePort;

    //! Number of calls to countingTime()
    U32 m_timeReads;

    //! Assert the stub HAL recorded the given call counts
    void assertHal(U32 disableInterrupts, U32 delays, U32 resets);
};

}  // namespace Samd21

#endif

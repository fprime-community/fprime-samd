// ======================================================================
// \title  FatalHandlerTester.cpp
// \author tumbar
// \brief  cpp file for FatalHandler test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/FatalHandler/test/ut/FatalHandlerTester.hpp"
#include "Fw/Log/LogPacket.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "Fw/Types/assert_hook.hpp"
#include "STest/Pick/Pick.hpp"
#include "samd-config/FatalHandlerConfig.hpp"

namespace Samd21 {

//! File argument passed to the assert hook; a file name under FW_FILENAME_ASSERT
//! (the UT assert level), a file ID otherwise
#if FW_ASSERT_LEVEL == FW_FILEID_ASSERT
static constexpr FILE_NAME_ARG TEST_FILE = 0x1234ABCD;
static constexpr U32 EXPECTED_FILE = 0x1234ABCD;
#else
static constexpr FILE_NAME_ARG TEST_FILE = "FatalHandlerTester.cpp";
//! File names do not fit the packet's U32 file field and are reported as 0
static constexpr U32 EXPECTED_FILE = 0;
#endif

//! Marker the stub HAL prints from reset(), matched by death tests
static constexpr const char* RESET_MARKER = "FatalHandlerHal::reset";

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FatalHandlerTester::FatalHandlerTester()
    : FatalHandlerGTestBase("FatalHandlerTester", FatalHandlerTester::MAX_HISTORY_SIZE),
      component("FatalHandler"),
      m_timeReads(0) {
    this->initComponents();
    this->connectPorts();
    FatalHandlerHardware::resetFatalHandlerState();
}

FatalHandlerTester::~FatalHandlerTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void FatalHandlerTester::resetTest() {
    this->clearHistory();
    FatalHandlerHardware::resetFatalHandlerState();
}

FatalHandlerTester::AssertArgs FatalHandlerTester::pickArgs(FwSizeType numArgs) {
    AssertArgs a = {};
    a.lineNo = STest::Pick::any();
    a.numArgs = numArgs;
    for (FwSizeType i = 0; i < numArgs; i++) {
        // The packet carries each argument as an I32
        a.args[i] = static_cast<FwAssertArgType>(static_cast<I32>(STest::Pick::any()));
    }
    return a;
}

Fw::Time FatalHandlerTester::pickTime() {
    const Fw::Time time(TimeBase::TB_PROC_TIME, 0, STest::Pick::any(), STest::Pick::startLength(0, 1000000));
    this->setTestTime(time);
    return time;
}

void FatalHandlerTester::invokeReport(const AssertArgs& a) {
    CHAR msg[FW_ASSERT_TEXT_SIZE] = {0};
    Fw::formatAssertMessage(TEST_FILE, a.lineNo, a.numArgs, a.args[0], a.args[1], a.args[2], a.args[3], a.args[4],
                            a.args[5], msg, static_cast<FwSizeType>(sizeof(msg)));
    Fw::printAssertMessage(msg);
}

void FatalHandlerTester::assertPacketsSent(const AssertArgs& a, FwEventIdType id, const Fw::Time& time) {
    // Reported directly on fatalOut, never through the event port
    ASSERT_EVENTS_SIZE(0);
    ASSERT_from_fatalOut_SIZE(FatalHandlerConfig::PACKET_REPEAT);

    for (FwSizeType i = 0; i < this->fromPortHistory_fatalOut->size(); i++) {
        const FromPortEntry_fatalOut& entry = this->fromPortHistory_fatalOut->at(i);
        ASSERT_EQ(entry.context, 0U);
        // Every copy is the same packet
        ASSERT_EQ(entry.data, this->fromPortHistory_fatalOut->at(0).data);

        Fw::ComBuffer data = entry.data;
        Fw::LogPacket packet;
        ASSERT_EQ(data.deserializeTo(packet), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(data.getDeserializeSizeLeft(), 0U);
        ASSERT_EQ(packet.getId(), id);
        ASSERT_EQ(packet.getTimeTag(), time);

        // Decode the arguments the way the ground does, through the dictionary's
        // AssertionFailed event
        this->dispatchEvents(packet.getId(), packet.getTimeTag(), Fw::LogSeverity::FATAL, packet.getLogBuffer());
    }

    ASSERT_EVENTS_AssertionFailed_SIZE(FatalHandlerConfig::PACKET_REPEAT);
    for (FwSizeType i = 0; i < FatalHandlerConfig::PACKET_REPEAT; i++) {
        ASSERT_EVENTS_AssertionFailed(i, EXPECTED_FILE, a.lineNo, static_cast<FwSizeStoreType>(a.numArgs),
                                      static_cast<I32>(a.args[0]), static_cast<I32>(a.args[1]),
                                      static_cast<I32>(a.args[2]), static_cast<I32>(a.args[3]),
                                      static_cast<I32>(a.args[4]), static_cast<I32>(a.args[5]));
    }
}

void FatalHandlerTester::countingTime(Fw::PassiveComponentBase* callComp, FwIndexType portNum, Fw::Time& time) {
    FatalHandlerTester* const tester = static_cast<FatalHandlerTester*>(callComp);
    tester->m_timeReads++;
    time.set(TimeBase::TB_PROC_TIME, 0, tester->m_timeReads, 0);
}

void FatalHandlerTester::assertHal(U32 disableInterrupts, U32 delays, U32 resets) {
    const FatalHandlerHardware::FatalHandlerState& state = FatalHandlerHardware::getFatalHandlerState();
    ASSERT_EQ(state.disable_interrupts_count, disableInterrupts);
    ASSERT_EQ(state.delay_count, delays);
    ASSERT_EQ(state.delay_total_ms, delays * FatalHandlerConfig::PACKET_DELAY_MS);
    ASSERT_EQ(state.reset_count, resets);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FatalHandlerTester::testNoDynamicHookRegistered() {
    REQUIREMENT(
        "SAMD21-FATAL-001: FatalHandler shall receive failed assertions through the link-time selected "
        "Fw_AssertHook implementation without registering an Fw::AssertHook");

    ASSERT_EQ(Fw::AssertHook::getRegisteredHook(), nullptr);
}

void FatalHandlerTester::testReportSendsAssertionPacket() {
    REQUIREMENT(
        "SAMD21-FATAL-002: FatalHandler shall send an AssertionFailed packet out of fatalOut "
        "PACKET_REPEAT times, PACKET_DELAY_MS apart, with interrupts disabled");

    const Fw::Time time = this->pickTime();

    this->resetTest();
    const AssertArgs a = pickArgs(6);
    invokeReport(a);

    this->assertPacketsSent(a, FatalHandlerComponentBase::EVENTID_ASSERTIONFAILED, time);
    this->assertHal(1, FatalHandlerConfig::PACKET_REPEAT, 0);
}

void FatalHandlerTester::testReportUsesIdBase() {
    REQUIREMENT("SAMD21-FATAL-003: The AssertionFailed packet shall carry the instance's AssertionFailed event ID");

    const FwEventIdType base = STest::Pick::lowerUpper(1, 0x7FFFFFFF);
    this->component.setIdBase(base);
    // dispatchEvents() decodes event IDs against the tester's own base
    this->setIdBase(base);

    const Fw::Time time = this->pickTime();

    this->resetTest();
    const AssertArgs a = pickArgs(2);
    invokeReport(a);

    this->assertPacketsSent(a, base + FatalHandlerComponentBase::EVENTID_ASSERTIONFAILED, time);
}

void FatalHandlerTester::testReportedArgumentCounts() {
    COMMENT("Every argument count from 0 to 6 is carried in the packet");

    for (FwSizeType numArgs = 0; numArgs <= 6; numArgs++) {
        // The recursion guard latches after the first report, so use a fresh
        // component for each count
        FatalHandlerTester tester;
        const Fw::Time time = tester.pickTime();
        tester.resetTest();
        const AssertArgs a = pickArgs(numArgs);
        invokeReport(a);
        tester.assertPacketsSent(a, FatalHandlerComponentBase::EVENTID_ASSERTIONFAILED, time);
    }
}

void FatalHandlerTester::testTimeReadOnce() {
    REQUIREMENT(
        "SAMD21-FATAL-010: Every copy of the AssertionFailed packet shall carry the time read once from timeGet "
        "when the assertion is reported");

    // A time source that advances on every read
    this->m_countingTimePort.init();
    this->m_countingTimePort.addCallComp(this, countingTime);
    this->component.set_timeGet_OutputPort(0, &this->m_countingTimePort);

    this->resetTest();
    const AssertArgs a = pickArgs(4);
    invokeReport(a);

    ASSERT_EQ(this->m_timeReads, 1U);
    this->assertPacketsSent(a, FatalHandlerComponentBase::EVENTID_ASSERTIONFAILED,
                            Fw::Time(TimeBase::TB_PROC_TIME, 0, 1, 0));
}

void FatalHandlerTester::testLatestInstanceReports() {
    REQUIREMENT(
        "SAMD21-FATAL-004: Failed assertions shall be reported through the most recently constructed FatalHandler");

    // A second, unconnected instance takes over from the tester's component
    FatalHandler other("other");
    other.init(TEST_INSTANCE_ID);

    this->resetTest();
    invokeReport(pickArgs(1));

    ASSERT_from_fatalOut_SIZE(0);
}

void FatalHandlerTester::testNoInstanceResetsSilently() {
    REQUIREMENT(
        "SAMD21-FATAL-005: A failed assertion raised while no FatalHandler exists shall reset the MCU "
        "without reporting");

    {
        // Destroying the reporting instance leaves none
        FatalHandler other("other");
    }

    this->resetTest();
    invokeReport(pickArgs(3));
    Fw::terminateOnAssert();

    ASSERT_from_fatalOut_SIZE(0);
    this->assertHal(1, 0, 1);
}

void FatalHandlerTester::testRecursiveAssertResets() {
    REQUIREMENT(
        "SAMD21-FATAL-006: An assertion raised while an assertion is being reported shall reset the MCU "
        "without sending further packets");

    this->resetTest();
    invokeReport(pickArgs(1));
    ASSERT_from_fatalOut_SIZE(FatalHandlerConfig::PACKET_REPEAT);

    this->resetTest();
    invokeReport(pickArgs(1));
    ASSERT_from_fatalOut_SIZE(0);
    this->assertHal(1, 0, 1);
}

void FatalHandlerTester::testReportWithoutFatalOut() {
    REQUIREMENT("SAMD21-FATAL-007: FatalHandler shall not send the packet when fatalOut is unconnected");

    FatalHandler unconnected("unconnected");
    unconnected.init(TEST_INSTANCE_ID);

    this->resetTest();
    invokeReport(pickArgs(0));

    ASSERT_from_fatalOut_SIZE(0);
    this->assertHal(1, 0, 0);
}

void FatalHandlerTester::testTerminateResets() {
    REQUIREMENT("SAMD21-FATAL-008: FatalHandler shall reset the MCU after reporting an assertion");

    this->resetTest();
    Fw::terminateOnAssert();

    ASSERT_from_fatalOut_SIZE(0);
    this->assertHal(0, 0, 1);
}

void FatalHandlerTester::testFwAssertRoutedToHandler() {
    COMMENT("FW_ASSERT reaches FatalHandler through the link-time hook, which resets before the framework aborts");

    // The framework aborts after terminateOnAssert() (FW_ASSERTIONS_ALWAYS_ABORT), so
    // the full path can only be observed in a death test
    ASSERT_DEATH_IF_SUPPORTED(FW_ASSERT(0, 1, 2), RESET_MARKER);
}

void FatalHandlerTester::testFatalReceiveAsserts() {
    REQUIREMENT("SAMD21-FATAL-009: FatalHandler shall assert when it receives a FATAL event");

    const FwIndexType portNum = 0;
    const FwEventIdType id = STest::Pick::any();
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_FatalReceive(portNum, id), RESET_MARKER);
}

}  // namespace Samd21

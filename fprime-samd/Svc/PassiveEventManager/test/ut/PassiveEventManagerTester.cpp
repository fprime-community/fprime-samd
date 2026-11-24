// ======================================================================
// \title  PassiveEventManagerTester.cpp
// \author tumbar
// \brief  cpp file for PassiveEventManager component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveEventManager/test/ut/PassiveEventManagerTester.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"

namespace Samd21 {

constexpr FwSizeType PassiveEventManagerTester::PACKET_HEADER_SIZE;
constexpr FwSizeType PassiveEventManagerTester::MAX_FITTING_ARGS;

static_assert(PassiveEventManagerTester::MAX_FITTING_ARGS < FW_LOG_BUFFER_MAX_SIZE,
              "A maximum-size LogBuffer must not fit in a com buffer, or the oversized tests are vacuous");

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PassiveEventManagerTester::PassiveEventManagerTester()
    : PassiveEventManagerGTestBase("PassiveEventManagerTester", PassiveEventManagerTester::MAX_HISTORY_SIZE),
      component("PassiveEventManager"),
      m_packetsAtAnnounce(0) {
    this->initComponents();
    this->connectPorts();
}

PassiveEventManagerTester::~PassiveEventManagerTester() {}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

void PassiveEventManagerTester::from_FatalAnnounce_handler(FwIndexType portNum, FwEventIdType Id) {
    this->m_packetsAtAnnounce = this->fromPortHistory_PktSend->size();
    this->pushFromPortEntry_FatalAnnounce(Id);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void PassiveEventManagerTester::resetTest() {
    this->clearHistory();
    this->m_packetsAtAnnounce = 0;
}

void PassiveEventManagerTester::fillArgs(Fw::LogBuffer& args, FwSizeType size) {
    ASSERT_LE(size, args.getCapacity());
    args.resetSer();
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(args.serializeFrom(static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF))), Fw::FW_SERIALIZE_OK);
    }
}

Fw::Time PassiveEventManagerTester::pickTime() {
    return Fw::Time(static_cast<TimeBase::T>(STest::Pick::lowerUpper(TimeBase::TB_NONE, TimeBase::TB_J2000_TIME)),
                    static_cast<FwTimeContextStoreType>(STest::Pick::lowerUpper(0, 0xFF)), STest::Pick::any(),
                    STest::Pick::lowerUpper(0, 999999));
}

Fw::LogSeverity PassiveEventManagerTester::pickNonFatal() {
    return static_cast<Fw::LogSeverity::T>(
        STest::Pick::lowerUpper(Fw::LogSeverity::WARNING_HI, Fw::LogSeverity::DIAGNOSTIC));
}

void PassiveEventManagerTester::sendEvent(FwEventIdType id,
                                      const Fw::Time& time,
                                      Fw::LogSeverity severity,
                                      const Fw::LogBuffer& args) {
    Fw::Time timeTag(time);
    Fw::LogBuffer argsCopy(args);
    this->invoke_to_LogRecv(0, id, timeTag, severity, argsCopy);
}

void PassiveEventManagerTester::assertPacket(FwSizeType index,
                                         FwEventIdType id,
                                         const Fw::Time& time,
                                         const Fw::LogBuffer& args) {
    ASSERT_LT(index, this->fromPortHistory_PktSend->size());
    const FromPortEntry_PktSend& entry = this->fromPortHistory_PktSend->at(index);
    ASSERT_EQ(entry.context, 0U);

    Fw::ComBuffer packet(entry.data);
    ASSERT_EQ(packet.getSize(), PACKET_HEADER_SIZE + args.getSize());

    FwPacketDescriptorType descriptor = 0;
    ASSERT_EQ(packet.deserializeTo(descriptor), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(descriptor, static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_LOG));

    FwEventIdType packetId = 0;
    ASSERT_EQ(packet.deserializeTo(packetId), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packetId, id);

    Fw::Time packetTime;
    ASSERT_EQ(packet.deserializeTo(packetTime), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packetTime, time);
    ASSERT_EQ(packetTime.getContext(), time.getContext());

    // The arguments follow with no length prefix, for the ground
    ASSERT_EQ(packet.getDeserializeSizeLeft(), args.getSize());
    for (FwSizeType i = 0; i < args.getSize(); i++) {
        U8 byte = 0;
        ASSERT_EQ(packet.deserializeTo(byte), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(byte, args.getBuffAddr()[i]) << "argument byte " << i;
    }
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PassiveEventManagerTester::testPacketLayout() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-001: PassiveEventManager shall send each event received on LogRecv out of PktSend "
        "as one F Prime log packet carrying the event ID, time tag and arguments");

    const FwEventIdType id = STest::Pick::any();
    const Fw::Time time = pickTime();
    Fw::LogBuffer args;
    fillArgs(args, STest::Pick::lowerUpper(1, MAX_FITTING_ARGS));

    this->resetTest();
    this->sendEvent(id, time, pickNonFatal(), args);
    ASSERT_from_PktSend_SIZE(1);
    ASSERT_from_FatalAnnounce_SIZE(0);
    this->assertPacket(0, id, time, args);
}

void PassiveEventManagerTester::testEverySeverity() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-002: PassiveEventManager shall send events of every severity, without filtering");

    for (FwIndexType s = Fw::LogSeverity::FATAL; s <= Fw::LogSeverity::DIAGNOSTIC; s++) {
        const FwEventIdType id = STest::Pick::any();
        const Fw::Time time = pickTime();
        Fw::LogBuffer args;
        fillArgs(args, STest::Pick::lowerUpper(0, MAX_FITTING_ARGS));

        this->resetTest();
        this->sendEvent(id, time, static_cast<Fw::LogSeverity::T>(s), args);
        ASSERT_from_PktSend_SIZE(1);
        this->assertPacket(0, id, time, args);
    }
}

void PassiveEventManagerTester::testArgumentSizes() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-003: PassiveEventManager shall send events whose arguments fit in a com buffer "
        "after the log packet header, from no arguments up to FW_COM_BUFFER_MAX_SIZE in total");

    for (FwSizeType size = 0; size <= MAX_FITTING_ARGS; size++) {
        const FwEventIdType id = STest::Pick::any();
        const Fw::Time time = pickTime();
        Fw::LogBuffer args;
        fillArgs(args, size);

        this->resetTest();
        this->sendEvent(id, time, pickNonFatal(), args);
        ASSERT_from_PktSend_SIZE(1);
        this->assertPacket(0, id, time, args);
    }
}

void PassiveEventManagerTester::testFatalAnnounced() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-004: PassiveEventManager shall announce each FATAL event's ID on FatalAnnounce, "
        "and shall not announce events of any other severity");

    const FwEventIdType id = STest::Pick::any();
    Fw::LogBuffer args;
    fillArgs(args, STest::Pick::lowerUpper(0, MAX_FITTING_ARGS));

    this->resetTest();
    this->sendEvent(id, pickTime(), Fw::LogSeverity::FATAL, args);
    ASSERT_from_FatalAnnounce_SIZE(1);
    ASSERT_from_FatalAnnounce(0, id);

    for (FwIndexType s = Fw::LogSeverity::WARNING_HI; s <= Fw::LogSeverity::DIAGNOSTIC; s++) {
        this->resetTest();
        this->sendEvent(STest::Pick::any(), pickTime(), static_cast<Fw::LogSeverity::T>(s), args);
        ASSERT_from_PktSend_SIZE(1);
        ASSERT_from_FatalAnnounce_SIZE(0);
    }
}

void PassiveEventManagerTester::testFatalPacketBeforeAnnounce() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-005: PassiveEventManager shall send a FATAL event's packet before announcing it, "
        "so the packet is out before the fatal handler takes over");

    const FwEventIdType id = STest::Pick::any();
    const Fw::Time time = pickTime();
    Fw::LogBuffer args;
    fillArgs(args, STest::Pick::lowerUpper(0, MAX_FITTING_ARGS));

    this->resetTest();
    this->sendEvent(id, time, Fw::LogSeverity::FATAL, args);
    ASSERT_from_FatalAnnounce_SIZE(1);
    ASSERT_EQ(this->m_packetsAtAnnounce, 1U);
    this->assertPacket(0, id, time, args);
}

void PassiveEventManagerTester::testRandomEvents() {
    COMMENT("Random events of random severity and size; every one is sent, only FATALs are announced");

    for (U32 i = 0; i < RANDOM_EVENTS; i++) {
        const FwEventIdType id = STest::Pick::any();
        const Fw::Time time = pickTime();
        const Fw::LogSeverity severity = static_cast<Fw::LogSeverity::T>(
            STest::Pick::lowerUpper(Fw::LogSeverity::FATAL, Fw::LogSeverity::DIAGNOSTIC));
        Fw::LogBuffer args;
        fillArgs(args, STest::Pick::lowerUpper(0, MAX_FITTING_ARGS));

        this->resetTest();
        this->sendEvent(id, time, severity, args);
        ASSERT_from_PktSend_SIZE(1);
        this->assertPacket(0, id, time, args);
        if (Fw::LogSeverity::FATAL == severity.e) {
            ASSERT_from_FatalAnnounce_SIZE(1);
            ASSERT_from_FatalAnnounce(0, id);
        } else {
            ASSERT_from_FatalAnnounce_SIZE(0);
        }
    }
}

void PassiveEventManagerTester::testOversizedEventDropped() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-006: PassiveEventManager shall drop, without asserting, an event whose log packet "
        "does not fit in a com buffer");

    for (FwSizeType size = MAX_FITTING_ARGS + 1; size <= FW_LOG_BUFFER_MAX_SIZE; size++) {
        Fw::LogBuffer args;
        fillArgs(args, size);

        this->resetTest();
        this->sendEvent(STest::Pick::any(), pickTime(), pickNonFatal(), args);
        ASSERT_from_PktSend_SIZE(0);
        ASSERT_from_FatalAnnounce_SIZE(0);
    }
}

void PassiveEventManagerTester::testOversizedFatalAnnounced() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-007: PassiveEventManager shall announce a FATAL event whose log packet does not fit "
        "in a com buffer, even though the packet is dropped");

    const FwEventIdType id = STest::Pick::any();
    Fw::LogBuffer args;
    fillArgs(args, FW_LOG_BUFFER_MAX_SIZE);

    this->resetTest();
    this->sendEvent(id, pickTime(), Fw::LogSeverity::FATAL, args);
    ASSERT_from_PktSend_SIZE(0);
    ASSERT_from_FatalAnnounce_SIZE(1);
    ASSERT_from_FatalAnnounce(0, id);
}

void PassiveEventManagerTester::testUnconnectedOutputs() {
    REQUIREMENT(
        "SAMD21-PASSIVEEVENTMANAGER-008: PassiveEventManager shall accept events, including FATAL events, with PktSend "
        "or FatalAnnounce unconnected");

    Fw::LogBuffer args;
    fillArgs(args, STest::Pick::lowerUpper(0, MAX_FITTING_ARGS));

    // Neither output connected
    PassiveEventManager bare("PassiveEventManagerBare");
    bare.init(TEST_INSTANCE_ID);
    this->connect_to_LogRecv(0, bare.get_LogRecv_InputPort(0));

    this->resetTest();
    this->sendEvent(STest::Pick::any(), pickTime(), Fw::LogSeverity::FATAL, args);
    this->sendEvent(STest::Pick::any(), pickTime(), pickNonFatal(), args);
    ASSERT_from_PktSend_SIZE(0);
    ASSERT_from_FatalAnnounce_SIZE(0);

    // Only PktSend connected: the FATAL packet is still sent
    PassiveEventManager sendOnly("PassiveEventManagerSendOnly");
    sendOnly.init(TEST_INSTANCE_ID);
    this->connect_to_LogRecv(0, sendOnly.get_LogRecv_InputPort(0));
    sendOnly.set_PktSend_OutputPort(0, this->get_from_PktSend(0));

    const FwEventIdType id = STest::Pick::any();
    const Fw::Time time = pickTime();
    this->resetTest();
    this->sendEvent(id, time, Fw::LogSeverity::FATAL, args);
    ASSERT_from_PktSend_SIZE(1);
    ASSERT_from_FatalAnnounce_SIZE(0);
    this->assertPacket(0, id, time, args);

    // Only FatalAnnounce connected: the FATAL is still announced
    PassiveEventManager announceOnly("PassiveEventManagerAnnounceOnly");
    announceOnly.init(TEST_INSTANCE_ID);
    this->connect_to_LogRecv(0, announceOnly.get_LogRecv_InputPort(0));
    announceOnly.set_FatalAnnounce_OutputPort(0, this->get_from_FatalAnnounce(0));

    this->resetTest();
    this->sendEvent(id, pickTime(), Fw::LogSeverity::FATAL, args);
    ASSERT_from_PktSend_SIZE(0);
    ASSERT_from_FatalAnnounce_SIZE(1);
    ASSERT_from_FatalAnnounce(0, id);

    // Leave the tester pointing at the fully connected component
    this->connect_to_LogRecv(0, this->component.get_LogRecv_InputPort(0));
}

}  // namespace Samd21

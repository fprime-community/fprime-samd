// ======================================================================
// \title  FprimeRouterTester.cpp
// \author tumbar
// \brief  cpp file for FprimeRouter component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/FprimeRouter/test/ut/FprimeRouterTester.hpp"
#include <cstring>
#include "STest/Pick/Pick.hpp"

namespace Samd21 {

constexpr U8 FprimeRouterTester::TEST_VC_ID;
constexpr FwSizeType FprimeRouterTester::PACKET_CAPACITY;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FprimeRouterTester::FprimeRouterTester(Optional optional)
    : FprimeRouterGTestBase("FprimeRouterTester", FprimeRouterTester::MAX_HISTORY_SIZE),
      component("FprimeRouter"),
      m_packet() {
    this->initComponents();
    if (optional == Optional::CONNECTED) {
        this->connectPorts();
    } else {
        this->connectPortsExceptOptional();
    }
}

FprimeRouterTester::~FprimeRouterTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FprimeRouterTester::testRouteCommand() {
    const FwSizeType size = STest::Pick::lowerUpper(1, FW_COM_BUFFER_MAX_SIZE);
    Fw::Buffer sent = this->sendPacket(ComCfg::Apid::FW_PACKET_COMMAND, size);

    this->assertRouted(1, 0, 0, 1);
    this->assertCommand(0, sent);
    // The command was copied out, so the buffer goes straight back with an empty context
    ComCfg::FrameContext empty;
    this->assertReturned(0, sent, empty.get_vcId(), empty.get_apid());
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testRouteCommandMaxSize() {
    Fw::Buffer sent = this->sendPacket(ComCfg::Apid::FW_PACKET_COMMAND, FW_COM_BUFFER_MAX_SIZE);

    this->assertRouted(1, 0, 0, 1);
    this->assertCommand(0, sent);
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testRouteCommandOversize() {
    const FwSizeType size = STest::Pick::lowerUpper(FW_COM_BUFFER_MAX_SIZE + 1, PACKET_CAPACITY);
    Fw::Buffer sent = this->sendPacket(ComCfg::Apid::FW_PACKET_COMMAND, size);

    // Too large for a ComBuffer: dropped with an event, buffer still returned
    this->assertRouted(0, 0, 0, 1);
    ComCfg::FrameContext empty;
    this->assertReturned(0, sent, empty.get_vcId(), empty.get_apid());
    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SerializationError_SIZE(1);
    ASSERT_EVENTS_SerializationError(0, static_cast<U32>(Fw::FW_SERIALIZE_NO_ROOM_LEFT));
}

void FprimeRouterTester::testRouteCommandOversizeThrottle() {
    const U32 throttle = FprimeRouterComponentBase::EVENTID_SERIALIZATIONERROR_THROTTLE;
    this->setTestTime(Fw::Time(TimeBase::TB_NONE, 100, 0));

    // A burst of oversize commands reports only the first THROTTLE of them. sendPacket()
    // clears the history, so check each packet on its own: every buffer is still returned.
    for (U32 i = 0; i < throttle + 3; i++) {
        this->sendPacket(ComCfg::Apid::FW_PACKET_COMMAND, FW_COM_BUFFER_MAX_SIZE + 1);
        this->assertRouted(0, 0, 0, 1);
        ASSERT_EVENTS_SerializationError_SIZE((i < throttle) ? 1 : 0);
    }

    // Within the 60 s window the event stays throttled
    this->setTestTime(Fw::Time(TimeBase::TB_NONE, 159, 0));
    this->sendPacket(ComCfg::Apid::FW_PACKET_COMMAND, FW_COM_BUFFER_MAX_SIZE + 1);
    this->assertRouted(0, 0, 0, 1);
    ASSERT_EVENTS_SerializationError_SIZE(0);

    // Once the window has passed it is reported again
    this->setTestTime(Fw::Time(TimeBase::TB_NONE, 160, 0));
    this->sendPacket(ComCfg::Apid::FW_PACKET_COMMAND, FW_COM_BUFFER_MAX_SIZE + 1);
    this->assertRouted(0, 0, 0, 1);
    ASSERT_EVENTS_SerializationError_SIZE(1);
}

void FprimeRouterTester::testRouteFile() {
    const FwSizeType size = STest::Pick::lowerUpper(1, PACKET_CAPACITY);
    Fw::Buffer sent = this->sendPacket(ComCfg::Apid::FW_PACKET_FILE, size);

    // Ownership passes to the file receiver; nothing returned yet
    this->assertRouted(0, 1, 0, 0);
    ASSERT_from_fileOut(0, sent);
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testRouteFileUnconnected() {
    const FwSizeType size = STest::Pick::lowerUpper(1, PACKET_CAPACITY);
    Fw::Buffer sent = this->sendPacket(ComCfg::Apid::FW_PACKET_FILE, size);

    // No file receiver: returned immediately with the context it arrived with
    this->assertRouted(0, 0, 0, 1);
    this->assertReturned(0, sent, TEST_VC_ID, ComCfg::Apid::FW_PACKET_FILE);
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testRouteUnknown() {
    // Every APID other than command and file is forwarded on unknownDataOut
    const ComCfg::Apid::T apids[] = {
        ComCfg::Apid::FW_PACKET_TELEM, ComCfg::Apid::FW_PACKET_LOG,     ComCfg::Apid::FW_PACKET_PACKETIZED_TLM,
        ComCfg::Apid::FW_PACKET_DP,    ComCfg::Apid::FW_PACKET_IDLE,    ComCfg::Apid::SPP_IDLE_PACKET,
        ComCfg::Apid::FW_PACKET_HAND,  ComCfg::Apid::FW_PACKET_UNKNOWN, ComCfg::Apid::INVALID_UNINITIALIZED,
    };
    for (const ComCfg::Apid::T apid : apids) {
        this->clearHistory();
        const FwSizeType size = STest::Pick::lowerUpper(1, PACKET_CAPACITY);
        Fw::Buffer sent = this->sendPacket(apid, size);

        this->assertRouted(0, 0, 1, 0);
        ASSERT_EQ(this->fromPortHistory_unknownDataOut->at(0).data, sent);
        ASSERT_EQ(this->fromPortHistory_unknownDataOut->at(0).context.get_vcId(), TEST_VC_ID);
        ASSERT_EQ(this->fromPortHistory_unknownDataOut->at(0).context.get_apid(), apid);
        ASSERT_EVENTS_SIZE(0);
    }
}

void FprimeRouterTester::testRouteUnknownUnconnected() {
    const FwSizeType size = STest::Pick::lowerUpper(1, PACKET_CAPACITY);
    Fw::Buffer sent = this->sendPacket(ComCfg::Apid::FW_PACKET_UNKNOWN, size);

    // No receiver: returned immediately with the context it arrived with
    this->assertRouted(0, 0, 0, 1);
    this->assertReturned(0, sent, TEST_VC_ID, ComCfg::Apid::FW_PACKET_UNKNOWN);
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testBufferReturn() {
    // A file and an unknown packet are handed off, then returned on fileBufferReturnIn
    // out of order. Each goes back on dataReturnOut, with an empty context.
    Fw::Buffer file = this->sendPacket(ComCfg::Apid::FW_PACKET_FILE, 8);
    Fw::Buffer unknown(this->m_packet + 8, 8);
    ComCfg::FrameContext context;
    context.set_apid(ComCfg::Apid::FW_PACKET_UNKNOWN);
    context.set_vcId(TEST_VC_ID);
    this->invoke_to_dataIn(0, unknown, context);
    this->assertRouted(0, 1, 1, 0);

    this->clearHistory();
    this->invoke_to_fileBufferReturnIn(0, unknown);
    this->invoke_to_fileBufferReturnIn(0, file);

    ComCfg::FrameContext empty;
    this->assertRouted(0, 0, 0, 2);
    this->assertReturned(0, unknown, empty.get_vcId(), empty.get_apid());
    this->assertReturned(1, file, empty.get_vcId(), empty.get_apid());
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testCommandResponse() {
    this->clearHistory();
    const FwOpcodeType opcode = STest::Pick::any();
    const U32 cmdSeq = STest::Pick::any();
    this->invoke_to_cmdResponseIn(0, opcode, cmdSeq, Fw::CmdResponse::OK);
    this->invoke_to_cmdResponseIn(0, opcode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);

    // Command responses are accepted and ignored
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void FprimeRouterTester::testRandomRouting() {
    // Every buffer received is returned exactly once: commands immediately, handed-off
    // buffers once the receiver gives them back.
    const ComCfg::Apid::T apids[] = {ComCfg::Apid::FW_PACKET_COMMAND, ComCfg::Apid::FW_PACKET_FILE,
                                     ComCfg::Apid::FW_PACKET_UNKNOWN, ComCfg::Apid::FW_PACKET_TELEM};
    for (U32 i = 0; i < 1000; i++) {
        const ComCfg::Apid::T apid = apids[STest::Pick::startLength(0, FW_NUM_ARRAY_ELEMENTS(apids))];
        const FwSizeType size = STest::Pick::lowerUpper(1, FW_COM_BUFFER_MAX_SIZE);
        Fw::Buffer sent = this->sendPacket(apid, size);

        if (apid == ComCfg::Apid::FW_PACKET_COMMAND) {
            this->assertRouted(1, 0, 0, 1);
            this->assertCommand(0, sent);
        } else {
            const bool isFile = (apid == ComCfg::Apid::FW_PACKET_FILE);
            this->assertRouted(0, isFile ? 1 : 0, isFile ? 0 : 1, 0);
            this->invoke_to_fileBufferReturnIn(0, sent);
            ASSERT_from_dataReturnOut_SIZE(1);
        }
        ASSERT_EQ(this->fromPortHistory_dataReturnOut->at(0).data, sent);
        ASSERT_EVENTS_SIZE(0);
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void FprimeRouterTester::connectPortsExceptOptional() {
    // Connect special output ports
    this->component.set_logOut_OutputPort(0, this->get_from_logOut(0));
    this->component.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));
    // Connect typed input ports
    this->connect_to_cmdResponseIn(0, this->component.get_cmdResponseIn_InputPort(0));
    this->connect_to_dataIn(0, this->component.get_dataIn_InputPort(0));
    this->connect_to_fileBufferReturnIn(0, this->component.get_fileBufferReturnIn_InputPort(0));
    // Connect the required typed output ports
    this->component.set_commandOut_OutputPort(0, this->get_from_commandOut(0));
    this->component.set_dataReturnOut_OutputPort(0, this->get_from_dataReturnOut(0));
}

Fw::Buffer FprimeRouterTester::sendPacket(ComCfg::Apid::T apid, FwSizeType size) {
    FW_ASSERT(size <= PACKET_CAPACITY, static_cast<FwAssertArgType>(size));
    for (FwSizeType i = 0; i < size; i++) {
        this->m_packet[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    Fw::Buffer buffer(this->m_packet, size);
    ComCfg::FrameContext context;
    context.set_apid(apid);
    context.set_vcId(TEST_VC_ID);
    this->clearHistory();
    this->invoke_to_dataIn(0, buffer, context);
    return buffer;
}

void FprimeRouterTester::assertRouted(FwSizeType commands, FwSizeType files, FwSizeType unknown, FwSizeType returns) {
    ASSERT_from_commandOut_SIZE(commands);
    ASSERT_from_fileOut_SIZE(files);
    ASSERT_from_unknownDataOut_SIZE(unknown);
    ASSERT_from_dataReturnOut_SIZE(returns);
}

void FprimeRouterTester::assertReturned(FwSizeType index, const Fw::Buffer& buffer, U8 vcId, ComCfg::Apid::T apid) {
    ASSERT_LT(index, this->fromPortHistory_dataReturnOut->size());
    const auto& entry = this->fromPortHistory_dataReturnOut->at(index);
    ASSERT_EQ(entry.data, buffer);
    ASSERT_EQ(entry.context.get_vcId(), vcId);
    ASSERT_EQ(entry.context.get_apid(), apid);
}

void FprimeRouterTester::assertCommand(FwSizeType index, const Fw::Buffer& buffer) {
    ASSERT_LT(index, this->fromPortHistory_commandOut->size());
    const auto& entry = this->fromPortHistory_commandOut->at(index);
    ASSERT_EQ(entry.context, 0U);
    ASSERT_EQ(entry.data.getSize(), buffer.getSize());
    ASSERT_EQ(std::memcmp(entry.data.getBuffAddr(), buffer.getData(), buffer.getSize()), 0);
}

}  // namespace Samd21

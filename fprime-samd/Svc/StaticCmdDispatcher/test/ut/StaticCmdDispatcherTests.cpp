// ======================================================================
// \title  StaticCmdDispatcherTests.cpp
// \brief  StaticCmdDispatcher unit tests, run against the StaticTopology
//
// The dispatch table is the code static-cmd-dispatcher generates for the StaticTopology,
// so these tests cover the generator's output together with the component.
// ======================================================================

#include "test/StaticTopology/Top/StaticTopologyFixture.hpp"

#include "Fw/Cmd/CmdArgBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Test/UnitTest.hpp"

namespace StaticTopology {

// Local ids, mirrored from StaticCmdDispatcher.fpp (the generated ones are protected)
constexpr FwOpcodeType DISP_OPCODE_NO_OP = 0x0;
constexpr FwOpcodeType DISP_OPCODE_CLEAR_TRACKING = 0x1;
constexpr FwOpcodeType DISP_OPCODE_SET_EVENT_EMISSION = 0x2;
constexpr FwEventIdType DISP_EVENT_OPCODE_DISPATCHED = 0x0;
constexpr FwEventIdType DISP_EVENT_OPCODE_COMPLETED = 0x1;
constexpr FwEventIdType DISP_EVENT_OPCODE_ERROR = 0x2;
constexpr FwEventIdType DISP_EVENT_MALFORMED_COMMAND = 0x3;
constexpr FwEventIdType DISP_EVENT_INVALID_COMMAND = 0x4;
constexpr FwEventIdType DISP_EVENT_TOO_MANY_COMMANDS = 0x5;
constexpr FwEventIdType DISP_EVENT_NO_OP_RECEIVED = 0x6;
constexpr FwEventIdType DISP_EVENT_PORT_INDEX_OUT_OF_RANGE = 0x7;
constexpr FwEventIdType DISP_EVENT_UNEXPECTED_RESPONSE = 0x8;

class StaticCmdDispatcherTest : public StaticTopologyFixture {
  protected:
    void TearDown() override {
        // Leave no command outstanding and event emission enabled for the next test
        while (target.getHeldCount() > 0) {
            target.release(Fw::CmdResponse::OK);
        }
        setEventEmission(0, true);
        setEventEmission(1, true);
    }

    static FwOpcodeType dispOpcode(FwOpcodeType local) { return cmdDisp.getIdBase() + local; }

    static void setEventEmission(U8 port, bool enabled) {
        U8 args[2];
        FwSizeType size = serialize(args, sizeof(args), port);
        size += serialize(&args[size], sizeof(args) - size, enabled);
        ground.sendCommand(dispOpcode(DISP_OPCODE_SET_EVENT_EMISSION), args, size, 0);
    }

    static FwSizeType dispEvents(FwEventIdType local) { return eventLog.count(eventId(cmdDisp, local)); }
};

TEST_F(StaticCmdDispatcherTest, RoutesEachInstanceByBaseId) {
    REQUIREMENT("STATIC-CMD-001");
    COMMENT("an opcode reaches the instance whose base id it was generated from");
    const U32 before1 = target.getSetCount();
    const U32 before2 = target2.getSetCount();

    command(target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, static_cast<U32>(0xCAFEF00D), 7);
    EXPECT_EQ(target.getSetCount(), before1 + 1);
    EXPECT_EQ(target.getValue(), 0xCAFEF00DU);
    EXPECT_EQ(target2.getSetCount(), before2);

    command(target2.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, static_cast<U32>(0x12345678), 8);
    EXPECT_EQ(target2.getSetCount(), before2 + 1);
    EXPECT_EQ(target2.getValue(), 0x12345678U);
    EXPECT_EQ(target.getSetCount(), before1 + 1);

    ASSERT_EQ(ground.getResponseCount(), 2U);
    EXPECT_EQ(ground.getResponse(0).opCode, target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE);
    EXPECT_EQ(ground.getResponse(0).context, 7U);
    EXPECT_EQ(ground.getResponse(0).response, Fw::CmdResponse::OK);
    EXPECT_EQ(ground.getResponse(1).context, 8U);
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_DISPATCHED), 2U);
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_COMPLETED), 2U);
}

TEST_F(StaticCmdDispatcherTest, RoutesOwnCommands) {
    REQUIREMENT("STATIC-CMD-001");
    COMMENT("the dispatcher's own commands are in the generated table");
    commandNoArgs(dispOpcode(DISP_OPCODE_NO_OP), 3);
    ASSERT_EQ(ground.getResponseCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::OK);
    EXPECT_EQ(dispEvents(DISP_EVENT_NO_OP_RECEIVED), 1U);
}

TEST_F(StaticCmdDispatcherTest, UnknownOpcodes) {
    REQUIREMENT("STATIC-CMD-002");
    COMMENT("opcodes outside the generated table fail INVALID_OPCODE and reach no instance");
    const U32 before = target.getSetCount();
    // Next local opcode past each instance's last command, and an opcode with no instance
    const FwOpcodeType unknown[] = {
        target.getIdBase() + Target::LOCAL_OPCODE_HOLD + 1,
        dispOpcode(DISP_OPCODE_SET_EVENT_EMISSION + 1),
        0x0FFF,
        0xFFFFFFFE,
    };
    for (const FwOpcodeType opCode : unknown) {
        ground.reset();
        eventLog.reset();
        commandNoArgs(opCode, 11);
        ASSERT_EQ(ground.getResponseCount(), 1U) << std::hex << opCode;
        EXPECT_EQ(ground.getLastResponse().opCode, opCode);
        EXPECT_EQ(ground.getLastResponse().context, 11U);
        EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::INVALID_OPCODE);
        EXPECT_EQ(dispEvents(DISP_EVENT_INVALID_COMMAND), 1U);
    }
    EXPECT_EQ(target.getSetCount(), before);
}

TEST_F(StaticCmdDispatcherTest, ReservedOpcodeIsInvalid) {
    REQUIREMENT("STATIC-CMD-002");
    COMMENT("the opcode reserved for an unused tracking slot is never dispatched");
    commandNoArgs(0xFFFFFFFF, 1);
    ASSERT_EQ(ground.getResponseCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::INVALID_OPCODE);
}

TEST_F(StaticCmdDispatcherTest, ErrorResponses) {
    REQUIREMENT("STATIC-CMD-003");
    COMMENT("a component's error response reaches the sender and logs OpCodeError");
    const Fw::CmdResponse responses[] = {Fw::CmdResponse::EXECUTION_ERROR, Fw::CmdResponse::VALIDATION_ERROR,
                                         Fw::CmdResponse::BUSY, Fw::CmdResponse::FORMAT_ERROR};
    for (const Fw::CmdResponse response : responses) {
        ground.reset();
        eventLog.reset();
        command(target.getIdBase() + Target::LOCAL_OPCODE_RESPOND, response, 21);
        ASSERT_EQ(ground.getResponseCount(), 1U);
        EXPECT_EQ(ground.getLastResponse().response, response);
        EXPECT_EQ(ground.getLastResponse().context, 21U);
        EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_ERROR), 1U);
        EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_COMPLETED), 0U);
    }
}

TEST_F(StaticCmdDispatcherTest, MalformedPacket) {
    REQUIREMENT("STATIC-CMD-004");
    COMMENT("a packet that does not deserialize fails VALIDATION_ERROR");
    Fw::ComBuffer packet;
    // A descriptor and half an opcode
    ASSERT_EQ(packet.serializeFrom(static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_COMMAND)),
              Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.serializeFrom(static_cast<U8>(0x01)), Fw::FW_SERIALIZE_OK);
    ground.sendCommandPacket(packet, 5);
    ASSERT_EQ(ground.getResponseCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::VALIDATION_ERROR);
    EXPECT_EQ(ground.getLastResponse().context, 5U);
    EXPECT_EQ(dispEvents(DISP_EVENT_MALFORMED_COMMAND), 1U);
}

TEST_F(StaticCmdDispatcherTest, SequencerCmdPort) {
    REQUIREMENT("STATIC-CMD-001");
    COMMENT("commands on the Fw.Cmd sequencer port dispatch and respond on that port's status");
    U8 args[sizeof(U32)];
    Fw::CmdArgBuffer buffer;
    const FwSizeType size = serialize(args, sizeof(args), static_cast<U32>(42));
    ASSERT_EQ(buffer.serializeFrom(args, size, Fw::Serialization::OMIT_LENGTH), Fw::FW_SERIALIZE_OK);
    sequencer.sendCmd(target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, 99, buffer);
    EXPECT_EQ(target.getValue(), 42U);
    EXPECT_EQ(ground.getResponseCount(), 0U);
    ASSERT_EQ(sequencer.getResponseCount(), 1U);
    EXPECT_EQ(sequencer.getLastResponse().context, 99U);
    EXPECT_EQ(sequencer.getLastResponse().response, Fw::CmdResponse::OK);
}

TEST_F(StaticCmdDispatcherTest, OutstandingCommandsLimit) {
    REQUIREMENT("STATIC-CMD-005");
    COMMENT("past CMD_DISPATCHER_SEQUENCER_TABLE_SIZE outstanding commands, a command fails TooManyCommands");
    const FwOpcodeType hold = target.getIdBase() + Target::LOCAL_OPCODE_HOLD;
    for (U32 i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; i++) {
        commandNoArgs(hold, 100 + i);
    }
    ASSERT_EQ(target.getHeldCount(), static_cast<FwSizeType>(CMD_DISPATCHER_SEQUENCER_TABLE_SIZE));
    EXPECT_EQ(ground.getResponseCount(), 0U);

    commandNoArgs(hold, 200);
    EXPECT_EQ(target.getHeldCount(), static_cast<FwSizeType>(CMD_DISPATCHER_SEQUENCER_TABLE_SIZE));
    ASSERT_EQ(ground.getResponseCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().context, 200U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::EXECUTION_ERROR);
    EXPECT_EQ(dispEvents(DISP_EVENT_TOO_MANY_COMMANDS), 1U);

    // Completing the held commands returns each one's context, in order, and frees the slots
    for (U32 i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; i++) {
        target.release(Fw::CmdResponse::OK);
        EXPECT_EQ(ground.getLastResponse().context, 100 + i);
        EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::OK);
    }
    command(target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, static_cast<U32>(1), 300);
    EXPECT_EQ(ground.getLastResponse().context, 300U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::OK);
}

TEST_F(StaticCmdDispatcherTest, ClearTracking) {
    REQUIREMENT("STATIC-CMD-005");
    COMMENT("CLEAR_TRACKING frees the slots; a late response is reported as unexpected");
    // CLEAR_TRACKING is itself tracked, so leave it a slot
    const FwOpcodeType hold = target.getIdBase() + Target::LOCAL_OPCODE_HOLD;
    for (U32 i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE - 1; i++) {
        commandNoArgs(hold, i);
    }
    commandNoArgs(dispOpcode(DISP_OPCODE_CLEAR_TRACKING), 50);
    // CLEAR_TRACKING also clears its own entry, so its response is untracked: the sender gets
    // no response and UnexpectedCommandResponse is logged. Current behavior, pinned here.
    EXPECT_EQ(ground.getResponseCount(), 0U);
    EXPECT_EQ(dispEvents(DISP_EVENT_UNEXPECTED_RESPONSE), 1U);

    // The whole table is free again
    ground.reset();
    for (U32 i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; i++) {
        commandNoArgs(hold, 60 + i);
    }
    EXPECT_EQ(ground.getResponseCount(), 0U);
    EXPECT_EQ(dispEvents(DISP_EVENT_TOO_MANY_COMMANDS), 0U);

    // The commands held before the clear complete untracked
    eventLog.reset();
    for (U32 i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE - 1; i++) {
        target.release(Fw::CmdResponse::OK);
    }
    EXPECT_EQ(ground.getResponseCount(), 0U);
    EXPECT_EQ(dispEvents(DISP_EVENT_UNEXPECTED_RESPONSE),
              static_cast<FwSizeType>(CMD_DISPATCHER_SEQUENCER_TABLE_SIZE - 1));
}

TEST_F(StaticCmdDispatcherTest, ClearTrackingWhenFull) {
    REQUIREMENT("STATIC-CMD-005");
    COMMENT("with every slot taken, CLEAR_TRACKING cannot be tracked either and fails TooManyCommands");
    const FwOpcodeType hold = target.getIdBase() + Target::LOCAL_OPCODE_HOLD;
    for (U32 i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; i++) {
        commandNoArgs(hold, i);
    }
    commandNoArgs(dispOpcode(DISP_OPCODE_CLEAR_TRACKING), 50);
    ASSERT_EQ(ground.getResponseCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::EXECUTION_ERROR);
    EXPECT_EQ(dispEvents(DISP_EVENT_TOO_MANY_COMMANDS), 1U);
}

TEST_F(StaticCmdDispatcherTest, UnexpectedResponse) {
    REQUIREMENT("STATIC-CMD-005");
    COMMENT("a response for a command never dispatched is logged and dropped");
    target.respondUnexpected(0xDEAD);
    EXPECT_EQ(ground.getResponseCount(), 0U);
    EXPECT_EQ(dispEvents(DISP_EVENT_UNEXPECTED_RESPONSE), 1U);
}

TEST_F(StaticCmdDispatcherTest, EventEmission) {
    REQUIREMENT("STATIC-CMD-006");
    COMMENT("SET_EVENT_EMISSION silences dispatch/completion events for one sequence port");
    setEventEmission(0, false);
    eventLog.reset();
    command(target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, static_cast<U32>(1));
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_DISPATCHED), 0U);
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_COMPLETED), 0U);
    // Errors are still reported
    command(target.getIdBase() + Target::LOCAL_OPCODE_RESPOND, Fw::CmdResponse(Fw::CmdResponse::BUSY));
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_ERROR), 1U);

    // The other port is unaffected
    eventLog.reset();
    Fw::CmdArgBuffer args;
    ASSERT_EQ(args.serializeFrom(static_cast<U32>(2)), Fw::FW_SERIALIZE_OK);
    sequencer.sendCmd(target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, 0, args);
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_DISPATCHED), 1U);

    setEventEmission(0, true);
    eventLog.reset();
    command(target.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, static_cast<U32>(3));
    EXPECT_EQ(dispEvents(DISP_EVENT_OPCODE_DISPATCHED), 1U);
}

TEST_F(StaticCmdDispatcherTest, EventEmissionPortOutOfRange) {
    REQUIREMENT("STATIC-CMD-006");
    COMMENT("SET_EVENT_EMISSION on a port past the sequence ports fails EXECUTION_ERROR");
    setEventEmission(static_cast<U8>(CmdDispatcherSequencePorts), false);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::EXECUTION_ERROR);
    EXPECT_EQ(dispEvents(DISP_EVENT_PORT_INDEX_OUT_OF_RANGE), 1U);
}

}  // namespace StaticTopology

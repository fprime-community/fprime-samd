// ======================================================================
// \title  StaticTlmPacketizerTests.cpp
// \brief  StaticTlmPacketizer unit tests, run against the StaticTopology
//
// The packet layout is the code static-tlm-packetizer generates for the StaticTopology's
// packet set, so these tests cover the generator's output together with the component.
// ======================================================================

#include "test/StaticTopology/Top/StaticTopologyFixture.hpp"

#include "Fw/Com/ComPacket.hpp"
#include "Fw/Test/UnitTest.hpp"

namespace StaticTopology {

// Local ids, mirrored from StaticTlmPacketizer.fpp (the generated ones are protected)
constexpr FwOpcodeType TLM_OPCODE_SEND_PKT = 0x0;
constexpr FwEventIdType TLM_EVENT_PACKET_NOT_FOUND = 0x1;

// Packet ids of the StaticTopology's packet set (topology.fpp)
constexpr FwTlmPacketizeIdType PACKET_PRIMARY = 0;
constexpr FwTlmPacketizeIdType PACKET_TYPES = 5;
constexpr FwTlmPacketizeIdType PACKET_EMPTY = 9;

class StaticTlmPacketizerTest : public StaticTopologyFixture {
  protected:
    //! A packet split into its header and payload
    struct Decoded {
        FwPacketDescriptorType descriptor;
        FwTlmPacketizeIdType id;
        Fw::Time time;
        Fw::ExternalSerializeBuffer payload;
        FwSizeType payloadSize;
        U8 bytes[FW_COM_BUFFER_MAX_SIZE];

        Decoded() : descriptor(0), id(0), time(), payload(), payloadSize(0), bytes() {}
    };

    //! Decode the header of a packet the packetizer sent; payload is left to deserialize
    static void decode(const Fw::ComBuffer& packet, Decoded& out) {
        (void)::memcpy(out.bytes, packet.getBuffAddr(), packet.getSize());
        out.payload.setExtBuffer(out.bytes, packet.getSize());
        ASSERT_EQ(out.payload.setBuffLen(packet.getSize()), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(out.payload.deserializeTo(out.descriptor), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(out.payload.deserializeTo(out.id), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(out.payload.deserializeTo(out.time), Fw::FW_SERIALIZE_OK);
        out.payloadSize = out.payload.getDeserializeSizeLeft();
    }

    static void sendPacket(FwTlmPacketizeIdType id, U32 context = 0) {
        command(tlm.getIdBase() + TLM_OPCODE_SEND_PKT, id, context);
    }

    static void setValue(Target& instance, U32 value) {
        command(instance.getIdBase() + Target::LOCAL_OPCODE_SET_VALUE, value);
    }
};

TEST_F(StaticTlmPacketizerTest, PacketHeader) {
    REQUIREMENT("SAMD21-TLM-003");
    COMMENT("a packet is the packetized-telemetry descriptor, the packet id and the time, then the payload");
    const Fw::Time now(TimeBase::TB_PROC_TIME, 0, 1234, 5678);
    clock.set(now);
    sendPacket(PACKET_PRIMARY, 3);
    ASSERT_EQ(ground.getPacketCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::OK);
    EXPECT_EQ(ground.getLastResponse().context, 3U);

    Decoded packet;
    decode(ground.getLastPacket().data, packet);
    EXPECT_EQ(packet.descriptor, static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_PACKETIZED_TLM));
    EXPECT_EQ(packet.id, PACKET_PRIMARY);
    EXPECT_EQ(packet.time, now);
}

TEST_F(StaticTlmPacketizerTest, PrimaryLayout) {
    REQUIREMENT("SAMD21-TLM-001");
    COMMENT("each channel is at its offset in packet order; same channel ids on two instances are distinct");
    setValue(target, 0x11223344);
    setValue(target2, 0xA5A5A5A5);
    const U32 counter = target.getSetCount();

    sendPacket(PACKET_PRIMARY);
    Decoded packet;
    decode(ground.getLastPacket().data, packet);
    // target.Value (U32), target.Counter (U8), target2.Value (U32)
    ASSERT_EQ(packet.payloadSize, sizeof(U32) + sizeof(U8) + sizeof(U32));
    U32 value = 0;
    U8 count = 0;
    ASSERT_EQ(packet.payload.deserializeTo(value), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(value, 0x11223344U);
    ASSERT_EQ(packet.payload.deserializeTo(count), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(count, static_cast<U8>(counter));
    ASSERT_EQ(packet.payload.deserializeTo(value), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(value, 0xA5A5A5A5U);
}

TEST_F(StaticTlmPacketizerTest, TypesLayout) {
    REQUIREMENT("SAMD21-TLM-001");
    COMMENT("every channel type is laid out with its serialized size; a channel may be in several packets");
    StaticTopology::Pair pair;
    pair.set_a(0xBEEF);
    pair.set_b(-1234);
    StaticTopology::Triple triple;
    triple[0] = 7;
    triple[1] = 8;
    triple[2] = 9;
    target.writeChannels(1, true, StaticTopology::Mode::AUTO, 0.5f, -0x123456789A, pair, triple, 0xFFFF);
    setValue(target, 0x01020304);

    sendPacket(PACKET_TYPES);
    Decoded packet;
    decode(ground.getLastPacket().data, packet);
    EXPECT_EQ(packet.id, PACKET_TYPES);
    ASSERT_EQ(packet.payloadSize, sizeof(U8) + StaticTopology::Mode::SERIALIZED_SIZE + sizeof(F32) + sizeof(I64) +
                                      StaticTopology::Pair::SERIALIZED_SIZE + StaticTopology::Triple::SERIALIZED_SIZE +
                                      sizeof(U32));

    bool flag = false;
    StaticTopology::Mode mode;
    F32 ratio = 0;
    I64 big = 0;
    StaticTopology::Pair pairOut;
    StaticTopology::Triple tripleOut;
    U32 value = 0;
    ASSERT_EQ(packet.payload.deserializeTo(flag), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.payload.deserializeTo(mode), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.payload.deserializeTo(ratio), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.payload.deserializeTo(big), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.payload.deserializeTo(pairOut), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.payload.deserializeTo(tripleOut), Fw::FW_SERIALIZE_OK);
    ASSERT_EQ(packet.payload.deserializeTo(value), Fw::FW_SERIALIZE_OK);
    EXPECT_TRUE(flag);
    EXPECT_EQ(mode, StaticTopology::Mode::AUTO);
    EXPECT_EQ(ratio, 0.5f);
    EXPECT_EQ(big, -0x123456789A);
    EXPECT_EQ(pairOut, pair);
    EXPECT_EQ(tripleOut, triple);
    EXPECT_EQ(value, 0x01020304U);

    // The same channel is current in every packet holding it
    sendPacket(PACKET_PRIMARY);
    decode(ground.getLastPacket().data, packet);
    ASSERT_EQ(packet.payload.deserializeTo(value), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(value, 0x01020304U);
}

TEST_F(StaticTlmPacketizerTest, LatestValue) {
    REQUIREMENT("SAMD21-TLM-001");
    COMMENT("a packet carries the most recent value written to each channel");
    setValue(target, 1);
    setValue(target, 2);
    sendPacket(PACKET_PRIMARY);
    Decoded packet;
    decode(ground.getLastPacket().data, packet);
    U32 value = 0;
    ASSERT_EQ(packet.payload.deserializeTo(value), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(value, 2U);
}

TEST_F(StaticTlmPacketizerTest, OmittedChannels) {
    REQUIREMENT("SAMD21-TLM-001");
    COMMENT("a channel in no packet is accepted and dropped");
    StaticTopology::Pair pair;
    StaticTopology::Triple triple;
    target2.writeChannels(4, true, StaticTopology::Mode::ON, 1.0f, 1, pair, triple, 0x1234);
    target.writeChannels(4, false, StaticTopology::Mode::OFF, 1.0f, 1, pair, triple, 0x1234);
    EXPECT_EQ(eventLog.getCount(), 0U);
    EXPECT_EQ(ground.getPacketCount(), 0U);
}

TEST_F(StaticTlmPacketizerTest, EmptyPacket) {
    REQUIREMENT("SAMD21-TLM-003");
    COMMENT("a packet with no channels is sent as its header alone");
    sendPacket(PACKET_EMPTY);
    ASSERT_EQ(ground.getPacketCount(), 1U);
    EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::OK);
    Decoded packet;
    decode(ground.getLastPacket().data, packet);
    EXPECT_EQ(packet.id, PACKET_EMPTY);
    EXPECT_EQ(packet.payloadSize, 0U);
}

TEST_F(StaticTlmPacketizerTest, UnknownPacket) {
    REQUIREMENT("SAMD21-TLM-004");
    COMMENT("SEND_PKT of a packet id not in the packet set fails EXECUTION_ERROR and sends nothing");
    const FwTlmPacketizeIdType unknown[] = {1, 4, 6, 10, 0xFFFF};
    for (const FwTlmPacketizeIdType id : unknown) {
        ground.reset();
        eventLog.reset();
        sendPacket(id, 9);
        EXPECT_EQ(ground.getPacketCount(), 0U) << id;
        ASSERT_EQ(ground.getResponseCount(), 1U);
        EXPECT_EQ(ground.getLastResponse().response, Fw::CmdResponse::EXECUTION_ERROR);
        EXPECT_EQ(eventLog.count(eventId(tlm, TLM_EVENT_PACKET_NOT_FOUND)), 1U);
    }
}

TEST_F(StaticTlmPacketizerTest, PortTrigger) {
    REQUIREMENT("SAMD21-TLM-002");
    COMMENT("a pktSendIn call sends the packet whose id is the port number");
    setValue(target, 77);
    ground.reset();
    ground.triggerPacket(0);
    ASSERT_EQ(ground.getPacketCount(), 1U);
    EXPECT_EQ(ground.getResponseCount(), 0U);
    Decoded packet;
    decode(ground.getLastPacket().data, packet);
    EXPECT_EQ(packet.id, PACKET_PRIMARY);
    U32 value = 0;
    ASSERT_EQ(packet.payload.deserializeTo(value), Fw::FW_SERIALIZE_OK);
    EXPECT_EQ(value, 77U);
}

}  // namespace StaticTopology

// ======================================================================
// \title  FramerTester.cpp
// \author tumbar
// \brief  cpp file for Framer component test harness implementation class
// ======================================================================

#include "FramerTester.hpp"
#include <cstring>
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"
#include "Svc/FrameAccumulator/FrameDetector/FprimeFrameDetector.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"
#include "samd-config/FramerConfig.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FramerTester ::FramerTester(bool connectDriver)
    : FramerGTestBase("FramerTester", FramerTester::MAX_HISTORY_SIZE), component("Framer"), m_numSentBuffers(0) {
    this->initComponents();
    this->connectPorts();

    // Initialize sent buffer tracking
    for (FwIndexType i = 0; i < 2; i++) {
        m_sentBuffers[i].valid = false;
        m_sentBuffers[i].size = 0;
    }

    // Signal that driver is connected
    if (connectDriver) {
        invoke_to_drvConnected(0);
    }
}

FramerTester ::~FramerTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FramerTester ::testSinglePacket() {
    REQUIREMENT(
        "SAMD21-FRAMER-003: Framer shall accumulate the packets received between schedIn calls and send them together "
        "in one drvSendOut buffer");
    // Send a single packet and verify it's framed correctly
    const U8 testData[] = {0x01, 0x02, 0x03, 0x04};
    sendComBuffer(testData, sizeof(testData));

    // Verify no send yet (waiting for schedIn or explicit flush)
    ASSERT_from_drvSendOut_SIZE(0);
}

void FramerTester ::testMultiPacketAccumulation() {
    REQUIREMENT(
        "SAMD21-FRAMER-003: Framer shall accumulate the packets received between schedIn calls and send them together "
        "in one drvSendOut buffer");
    REQUIREMENT("SAMD21-FRAMER-004: Framer shall send the accumulated packets on drvSendOut when schedIn is invoked");
    // Send multiple small packets
    for (FwIndexType i = 0; i < 5; i++) {
        U8 data[10];
        for (FwIndexType j = 0; j < sizeof(data); j++) {
            data[j] = static_cast<U8>(i * 10 + j);
        }
        sendComBuffer(data, sizeof(data));
    }

    // Verify no send yet (accumulated in buffer)
    ASSERT_from_drvSendOut_SIZE(0);

    // Trigger flush via schedIn
    invoke_to_schedIn(0, 0);

    // Should have sent one frame with all packets
    ASSERT_from_drvSendOut_SIZE(1);
}

void FramerTester ::testBufferOverflow() {
    REQUIREMENT(
        "SAMD21-FRAMER-005: Framer shall send the accumulated packets on drvSendOut before accepting a packet that "
        "would not fit in FRAMER_TX_BUFFER_SIZE bytes");
    // Test that buffer auto-flushes when it fills up
    // Send many small packets until we trigger an overflow
    // With 256 byte buffer and ~50 byte packets, we should overflow around packet 5

    FwIndexType packetsSent = 0;
    const FwIndexType MAX_PACKETS = 10;

    // Keep sending until we get a flush
    for (packetsSent = 0; packetsSent < MAX_PACKETS; packetsSent++) {
        U8 data[50];
        for (FwIndexType j = 0; j < sizeof(data); j++) {
            data[j] = static_cast<U8>(packetsSent);
        }
        sendComBuffer(data, sizeof(data));

        // Check if auto-flush happened
        if (fromPortHistory_drvSendOut->size() > 0) {
            break;
        }
    }

    // Should have auto-flushed before filling all packets
    ASSERT_GT(packetsSent, 0);            // Sent at least one
    ASSERT_LT(packetsSent, MAX_PACKETS);  // But not all (overflow triggered)
    ASSERT_from_drvSendOut_SIZE(1);       // One buffer sent

    // The overflow packet was dropped (backpressure - buffer 0 still transmitting)
    // Return the first buffer so we can continue
    const Fw::Buffer& buf1Const = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    Fw::Buffer buf1(buf1Const.getData(), buf1Const.getSize());
    invoke_to_drvReturnIn(0, buf1, Drv::ByteStreamStatus::OP_OK);

    // Now send another packet and flush
    U8 more[50];
    for (FwIndexType j = 0; j < sizeof(more); j++) {
        more[j] = 0xFF;
    }
    sendComBuffer(more, sizeof(more));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);  // Second buffer sent
}

void FramerTester ::testDoubleBufferLifecycle() {
    REQUIREMENT(
        "SAMD21-FRAMER-006: Framer shall not write to a buffer sent on drvSendOut until that buffer is returned on "
        "drvReturnIn");
    // Fill and flush first buffer
    U8 data1[50];
    for (FwIndexType i = 0; i < sizeof(data1); i++) {
        data1[i] = 0xAA;
    }
    sendComBuffer(data1, sizeof(data1));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    // Return first buffer to free it up
    const Fw::Buffer& buf1Const = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    Fw::Buffer buf1(buf1Const.getData(), buf1Const.getSize());
    invoke_to_drvReturnIn(0, buf1, Drv::ByteStreamStatus::OP_OK);

    // Now fill and flush second buffer
    U8 data2[50];
    for (FwIndexType i = 0; i < sizeof(data2); i++) {
        data2[i] = 0xBB;
    }
    sendComBuffer(data2, sizeof(data2));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);

    // Return second buffer
    const Fw::Buffer& buf2Const = this->fromPortHistory_drvSendOut->at(1).fwBuffer;
    Fw::Buffer buf2(buf2Const.getData(), buf2Const.getSize());
    invoke_to_drvReturnIn(0, buf2, Drv::ByteStreamStatus::OP_OK);

    // Both buffers should now be available
    // Send another packet and flush
    sendComBuffer(data1, sizeof(data1));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);
}

void FramerTester ::testBackpressure() {
    REQUIREMENT(
        "SAMD21-FRAMER-006: Framer shall not write to a buffer sent on drvSendOut until that buffer is returned on "
        "drvReturnIn");
    REQUIREMENT("SAMD21-FRAMER-004: Framer shall send the accumulated packets on drvSendOut when schedIn is invoked");
    // Test backpressure behavior when both buffers are in flight
    U8 data[50];
    for (FwIndexType i = 0; i < sizeof(data); i++) {
        data[i] = 0xCC;
    }

    // Send and flush buffer 1
    sendComBuffer(data, sizeof(data));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    // Return buffer 1 so we can use buffer 2
    const Fw::Buffer& buf1Const = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    Fw::Buffer buf1(buf1Const.getData(), buf1Const.getSize());
    invoke_to_drvReturnIn(0, buf1, Drv::ByteStreamStatus::OP_OK);

    // Send and flush buffer 2
    sendComBuffer(data, sizeof(data));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);

    // Now buffer 2 is in flight, buffer 1 is idle
    // Send data to buffer 1, but DON'T flush yet
    sendComBuffer(data, sizeof(data));
    ASSERT_from_drvSendOut_SIZE(2);  // Still 2, not flushed

    // Try to flush - should not work because buffer 2 is still transmitting
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);  // Still 2 (backpressure prevents flush)

    // Return buffer 2 to free up space
    const Fw::Buffer& buf2Const = this->fromPortHistory_drvSendOut->at(1).fwBuffer;
    Fw::Buffer buf2(buf2Const.getData(), buf2Const.getSize());
    invoke_to_drvReturnIn(0, buf2, Drv::ByteStreamStatus::OP_OK);

    // Now we can flush buffer 1
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);  // Third send successful
}

void FramerTester ::testSchedInFlush() {
    REQUIREMENT("SAMD21-FRAMER-004: Framer shall send the accumulated packets on drvSendOut when schedIn is invoked");
    // Add data but don't overflow
    U8 data[50];
    for (FwIndexType i = 0; i < sizeof(data); i++) {
        data[i] = 0xDD;
    }
    sendComBuffer(data, sizeof(data));

    // No automatic flush
    ASSERT_from_drvSendOut_SIZE(0);

    // Trigger flush via schedIn
    invoke_to_schedIn(0, 0);

    // Should have flushed
    ASSERT_from_drvSendOut_SIZE(1);
}

void FramerTester ::testFrameFormat() {
    REQUIREMENT(
        "SAMD21-FRAMER-001: Framer shall wrap each packet in an F Prime protocol frame: a header carrying the "
        "0xDEADBEEF start word and the packet length, the packet, and a trailer carrying the CRC32 of the header "
        "and packet");
    REQUIREMENT(
        "SAMD21-FRAMER-003: Framer shall accumulate the packets received between schedIn calls and send them "
        "together in one drvSendOut buffer");

    // Packets of different sizes, including empty and maximum-size packets
    Fw::ComBuffer packets[3];
    packets[0] = makeComBuffer(0);
    packets[1] = makeComBuffer(FW_COM_BUFFER_MAX_SIZE, 0x5A);
    for (U8 i = 0; i < 7; i++) {
        ASSERT_EQ(packets[2].serializeFrom(&i, sizeof(i), Fw::Serialization::OMIT_LENGTH), Fw::FW_SERIALIZE_OK);
    }
    for (FwSizeType i = 0; i < 3; i++) {
        invoke_to_comPacketQueueIn(0, packets[i], 0);
    }
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);
    assertFrames(0, packets, 3);
}

void FramerTester ::testGroundDeframes() {
    REQUIREMENT(
        "SAMD21-FRAMER-002: Frames sent by Framer shall be accepted by the F Prime frame detector used to deframe "
        "the stream on the ground");

    const Fw::ComBuffer packet = makeComBuffer(37, 0x3C);
    Fw::ComBuffer copy = packet;
    invoke_to_comPacketQueueIn(0, copy, 0);
    invoke_to_comPacketQueueIn(0, copy, 0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    const Fw::Buffer& sent = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    const FwSizeType frameSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + packet.getSize() +
                                 Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    ASSERT_EQ(sent.getSize(), 2 * frameSize);

    U8 storage[FramerConfig::FRAMER_TX_BUFFER_SIZE];
    Types::CircularBuffer circular(storage, sizeof(storage));
    ASSERT_EQ(circular.serialize(sent.getData(), sent.getSize()), Fw::FW_SERIALIZE_OK);

    // Both frames are detected back to back
    Svc::FrameDetectors::FprimeFrameDetector detector;
    for (FwSizeType i = 0; i < 2; i++) {
        FwSizeType detected = 0;
        ASSERT_EQ(detector.detect(circular, detected), Svc::FrameDetector::Status::FRAME_DETECTED);
        ASSERT_EQ(detected, frameSize);
        circular.rotate(detected);
    }

    // A corrupted byte is rejected
    U8 corrupt[FramerConfig::FRAMER_TX_BUFFER_SIZE];
    ::memcpy(corrupt, sent.getData(), frameSize);
    corrupt[Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + 3] ^= 0x10;
    Types::CircularBuffer corrupted(storage, sizeof(storage));
    ASSERT_EQ(corrupted.serialize(corrupt, frameSize), Fw::FW_SERIALIZE_OK);
    FwSizeType detected = 0;
    ASSERT_NE(detector.detect(corrupted, detected), Svc::FrameDetector::Status::FRAME_DETECTED);
}

void FramerTester ::testEmptySchedIn() {
    REQUIREMENT("SAMD21-FRAMER-004: Framer shall send the accumulated packets on drvSendOut when schedIn is invoked");

    // Nothing accumulated: nothing sent
    invoke_to_schedIn(0, 0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(0);

    // After a flush, the next schedIn has nothing new to send
    sendComBuffer(reinterpret_cast<const U8*>("abc"), 3);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);
    returnBuffer(0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);
}

void FramerTester ::testDroppedPacketTelemetry() {
    REQUIREMENT(
        "SAMD21-FRAMER-007: Framer shall drop a packet when no buffer has room for it and count it in the "
        "DroppedPackets channel, reported on each schedIn");

    const Fw::ComBuffer packet = makeComBuffer(FW_COM_BUFFER_MAX_SIZE, 0x11);
    const FwSizeType frameSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + packet.getSize() +
                                 Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    const FwSizeType perBuffer = FramerConfig::FRAMER_TX_BUFFER_SIZE / frameSize;
    ASSERT_GT(perBuffer, 0U);

    invoke_to_schedIn(0, 0);
    ASSERT_TLM_DroppedPackets_SIZE(1);
    ASSERT_TLM_DroppedPackets(0, 0);

    // Fill both buffers without returning either; the first is sent by the overflow
    Fw::ComBuffer copy;
    for (FwSizeType i = 0; i < 2 * perBuffer; i++) {
        copy = packet;
        invoke_to_comPacketQueueIn(0, copy, 0);
    }
    ASSERT_from_drvSendOut_SIZE(1);

    // The next overflow sends the second buffer too; with both in flight, packets are dropped
    const U32 DROPS = 3;
    for (U32 i = 0; i < DROPS; i++) {
        copy = packet;
        invoke_to_comPacketQueueIn(0, copy, 0);
    }
    ASSERT_from_drvSendOut_SIZE(2);

    // schedIn reports the drops
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);
    ASSERT_TLM_DroppedPackets_SIZE(2);
    ASSERT_TLM_DroppedPackets(1, DROPS);

    // Once both are returned, packets are accepted again and the count holds
    returnBuffer(0);
    returnBuffer(1);
    copy = packet;
    invoke_to_comPacketQueueIn(0, copy, 0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);
    ASSERT_TLM_DroppedPackets(2, DROPS);
}

void FramerTester ::testBeforeConnected() {
    REQUIREMENT("SAMD21-FRAMER-008: Framer shall not send on drvSendOut before drvConnected is invoked");
    REQUIREMENT(
        "SAMD21-FRAMER-007: Framer shall drop a packet when no buffer has room for it and count it in the "
        "DroppedPackets channel, reported on each schedIn");

    FramerTester tester(false);
    const Fw::ComBuffer packet = makeComBuffer(FW_COM_BUFFER_MAX_SIZE, 0x22);
    const FwSizeType frameSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + packet.getSize() +
                                 Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    const FwSizeType perBuffer = FramerConfig::FRAMER_TX_BUFFER_SIZE / frameSize;

    // Fill the buffer and overflow it: nothing is sent, the overflow is dropped
    Fw::ComBuffer copy;
    for (FwSizeType i = 0; i < perBuffer + 2; i++) {
        copy = packet;
        tester.invoke_to_comPacketQueueIn(0, copy, 0);
    }
    tester.invoke_to_schedIn(0, 0);
    ASSERT_EQ(tester.fromPortHistory_drvSendOut->size(), 0U);
    ASSERT_EQ(tester.tlmHistory_DroppedPackets->size(), 0U);

    // Once connected, the packets held from before are sent and the drops are reported
    tester.invoke_to_drvConnected(0);
    tester.invoke_to_schedIn(0, 0);
    ASSERT_EQ(tester.fromPortHistory_drvSendOut->size(), 1U);
    ASSERT_EQ(tester.fromPortHistory_drvSendOut->at(0).fwBuffer.getSize(), perBuffer * frameSize);
    ASSERT_EQ(tester.tlmHistory_DroppedPackets->size(), 1U);
    ASSERT_EQ(tester.tlmHistory_DroppedPackets->at(0).arg, 2U);
}

void FramerTester ::testReturnWithError() {
    REQUIREMENT(
        "SAMD21-FRAMER-006: Framer shall not write to a buffer sent on drvSendOut until that buffer is returned on "
        "drvReturnIn");
    REQUIREMENT(
        "SAMD21-FRAMER-009: Framer shall count the frames of a buffer returned on drvReturnIn with a failure status "
        "as dropped in the DroppedPackets channel");

    // A buffer returned with a failure status is freed just like a successful one, and the
    // frames it carried are counted as dropped. Round r carries r + 1 frames.
    Fw::ComBuffer packets[4];
    for (FwSizeType i = 0; i < 4; i++) {
        packets[i] = makeComBuffer(20, static_cast<U8>(0x70 + i));
    }
    const Drv::ByteStreamStatus::T statuses[4] = {Drv::ByteStreamStatus::SEND_RETRY, Drv::ByteStreamStatus::OP_OK,
                                                  Drv::ByteStreamStatus::OTHER_ERROR, Drv::ByteStreamStatus::OP_OK};
    U32 expectedDrops = 0;
    for (U32 round = 0; round < 4; round++) {
        for (U32 i = 0; i <= round; i++) {
            Fw::ComBuffer copy = packets[i];
            invoke_to_comPacketQueueIn(0, copy, 0);
        }
        invoke_to_schedIn(0, 0);
        ASSERT_from_drvSendOut_SIZE(round + 1);
        assertFrames(static_cast<FwIndexType>(round), packets, round + 1);
        ASSERT_TLM_DroppedPackets(round, expectedDrops);

        const Fw::Buffer& sent = this->fromPortHistory_drvSendOut->at(round).fwBuffer;
        Fw::Buffer buf(sent.getData(), sent.getSize());
        invoke_to_drvReturnIn(0, buf, statuses[round]);
        if (statuses[round] != Drv::ByteStreamStatus::OP_OK) {
            expectedDrops += round + 1;
        }
    }

    // The next report carries every lost frame
    invoke_to_schedIn(0, 0);
    ASSERT_TLM_DroppedPackets(4, expectedDrops);
    ASSERT_EQ(expectedDrops, 4U);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void FramerTester ::sendComBuffer(const U8* data, FwSizeType size) {
    Fw::ComBuffer comBuf;
    Fw::SerializeStatus status = comBuf.serializeFrom(data, size, Fw::Serialization::OMIT_LENGTH);
    ASSERT_EQ(status, Fw::FW_SERIALIZE_OK);
    invoke_to_comPacketQueueIn(0, comBuf, 0);
}

void FramerTester ::verifyFramedBuffer(FwSizeType expectedPayloadSize) {
    // Verify the sent buffer has correct framing
    ASSERT_from_drvSendOut_SIZE(1);
    const Fw::Buffer& sentBuf = this->fromPortHistory_drvSendOut->at(0).fwBuffer;

    // Minimum size is header + payload + trailer
    FwSizeType minSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + expectedPayloadSize +
                         Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    ASSERT_GE(sentBuf.getSize(), minSize);
}

void FramerTester ::returnBuffer(FwIndexType index) {
    ASSERT_LT(index, this->fromPortHistory_drvSendOut->size());
    const Fw::Buffer& bufConst = this->fromPortHistory_drvSendOut->at(index).fwBuffer;
    Fw::Buffer buf(bufConst.getData(), bufConst.getSize());
    invoke_to_drvReturnIn(0, buf, Drv::ByteStreamStatus::OP_OK);
}

Fw::ComBuffer FramerTester ::makeComBuffer(FwSizeType size, U8 fillValue) {
    Fw::ComBuffer comBuf;
    for (FwSizeType i = 0; i < size && i < FW_COM_BUFFER_MAX_SIZE; i++) {
        U8 val = fillValue;
        comBuf.serializeFrom(&val, sizeof(val), Fw::Serialization::OMIT_LENGTH);
    }
    return comBuf;
}

void FramerTester ::assertFrames(FwIndexType index, const Fw::ComBuffer* packets, FwSizeType count) {
    ASSERT_LT(index, this->fromPortHistory_drvSendOut->size());
    const Fw::Buffer& sent = this->fromPortHistory_drvSendOut->at(index).fwBuffer;
    const U8* bytes = sent.getData();
    FwSizeType offset = 0;

    for (FwSizeType i = 0; i < count; i++) {
        const FwSizeType size = packets[i].getSize();
        const FwSizeType headerSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE;
        ASSERT_LE(offset + headerSize + size + Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE, sent.getSize());

        Fw::ExternalSerializeBuffer header(const_cast<U8*>(bytes + offset), headerSize);
        header.setBuffLen(headerSize);
        Svc::FprimeProtocol::FrameHeader decoded;
        ASSERT_EQ(header.deserializeTo(decoded), Fw::FW_SERIALIZE_OK);
        ASSERT_EQ(decoded.get_startWord(), 0xDEADBEEFU);
        ASSERT_EQ(decoded.get_lengthField(), size);

        ASSERT_EQ(::memcmp(bytes + offset + headerSize, packets[i].getBuffAddr(), size), 0) << "packet " << i;

        Utils::HashBuffer hash;
        Utils::Hash::hash(bytes + offset, headerSize + size, hash);
        const U8* crc = bytes + offset + headerSize + size;
        const U32 crcField = (static_cast<U32>(crc[0]) << 24) | (static_cast<U32>(crc[1]) << 16) |
                             (static_cast<U32>(crc[2]) << 8) | static_cast<U32>(crc[3]);
        ASSERT_EQ(crcField, hash.asBigEndianU32());

        offset += headerSize + size + Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    }
    ASSERT_EQ(offset, sent.getSize());
}

}  // namespace Samd21

// ======================================================================
// \title  TmFramerTester.cpp
// \brief  cpp file for TmFramer component test harness implementation class
// ======================================================================

#include "TmFramerTester.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Svc/Ccsds/Types/SpacePacketHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/TMHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/TMTrailerSerializableAc.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

TmFramerTester ::TmFramerTester() : TmFramerGTestBase("TmFramerTester", TmFramerTester::MAX_HISTORY_SIZE), component("TmFramer") {
    this->initComponents();
    this->connectPorts();

    // Signal that driver is connected -- required before any flush can occur
    invoke_to_drvConnected(0);
}

TmFramerTester ::~TmFramerTester() {}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void TmFramerTester ::testNominalFraming() {
    const FwSizeType payloadSize = 20;
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, payloadSize);

    // Not sent yet -- comPacketQueueIn only accumulates, schedIn/overflow triggers the flush
    ASSERT_from_drvSendOut_SIZE(0);

    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    const Fw::Buffer& outBuffer = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    ASSERT_EQ(outBuffer.getSize(), static_cast<FwSizeType>(ComCfg::TmFrameFixedSize));

    U8* frameData = outBuffer.getData();
    ASSERT_EQ(getFrameScId(frameData), static_cast<U16>(ComCfg::SpacecraftId));
    ASSERT_EQ(getFrameVcId(frameData), 1);
    ASSERT_EQ(getFrameMcCount(frameData), 0);
    ASSERT_EQ(getFrameVcCount(frameData), 0);

    ASSERT_EQ(getSpacePacketApid(frameData, 0), static_cast<U16>(ComCfg::Apid::FW_PACKET_TELEM));
    ASSERT_EQ(getSpacePacketSeqCount(frameData, 0), 0);

    // Idle Space Packet fills the remainder: header + payload + idle Space Packet header,
    // then idle-fill pattern out to the trailer.
    const FwSizeType idleDataOffset = Svc::Ccsds::TMHeader::SERIALIZED_SIZE +
                                       Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + payloadSize +
                                       Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE;
    const FwSizeType idleDataEndOffset =
        static_cast<FwSizeType>(ComCfg::TmFrameFixedSize) - Svc::Ccsds::TMTrailer::SERIALIZED_SIZE;
    for (FwSizeType i = idleDataOffset; i < idleDataEndOffset; ++i) {
        ASSERT_EQ(frameData[i], TmFramer::IDLE_DATA_PATTERN) << "Idle data at index " << i << " does not match";
    }
}

void TmFramerTester ::testMultiPacketAccumulation() {
    for (FwIndexType i = 0; i < 5; i++) {
        sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10, static_cast<U8>(i));
    }
    ASSERT_from_drvSendOut_SIZE(0);

    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    // All 5 Space Packets landed in the same frame, back to back
    const U8* frameData = this->fromPortHistory_drvSendOut->at(0).fwBuffer.getData();
    for (FwIndexType i = 0; i < 5; i++) {
        FwSizeType offset = static_cast<FwSizeType>(i) * (Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + 10);
        ASSERT_EQ(getSpacePacketSeqCount(frameData, offset), static_cast<U16>(i));
    }
}

void TmFramerTester ::testDeadZoneAvoidance() {
    // Fill the buffer close to capacity using several max-size packets (a single ComBuffer is
    // capped at FW_COM_BUFFER_MAX_SIZE, so one packet alone can't approach dataFieldCapacity),
    // stopping just short of where the NEXT packet would leave a remainder smaller than a
    // valid idle Space Packet (SpacePacketHeader::SERIALIZED_SIZE + 1 = 7 bytes) but greater
    // than zero. comPacketQueueIn_handler checks this look-ahead BEFORE appending: rather than
    // appending into that dead zone, it must flush the accumulated buffer first and place the
    // triggering packet into the newly-active buffer instead.
    const FwSizeType dataFieldCapacity =
        static_cast<FwSizeType>(ComCfg::TmFrameFixedSize) -
        (Svc::Ccsds::TMHeader::SERIALIZED_SIZE + Svc::Ccsds::TMTrailer::SERIALIZED_SIZE);
    const FwSizeType idleMinSize = Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + 1;
    const FwSizeType maxPacketWireSize = Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + FW_COM_BUFFER_MAX_SIZE;

    FwSizeType used = 0;
    while (dataFieldCapacity - used > maxPacketWireSize + (idleMinSize - 1)) {
        sendPacket(ComCfg::Apid::FW_PACKET_TELEM, FW_COM_BUFFER_MAX_SIZE);
        used += maxPacketWireSize;
    }
    ASSERT_from_drvSendOut_SIZE(0);

    // Size this packet so that appending it would leave exactly idleMinSize - 1 bytes free --
    // the smallest possible dead zone. That must defer it to a flush instead.
    FwSizeType triggerComBufferSize =
        dataFieldCapacity - used - Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE - (idleMinSize - 1);
    sendPacket(ComCfg::Apid::FW_PACKET_LOG, triggerComBufferSize);
    ASSERT_from_drvSendOut_SIZE(1);

    // The flushed frame holds only the accumulated TELEM packets; the LOG trigger packet
    // landed in the newly-active buffer instead.
    const U8* frameData = this->fromPortHistory_drvSendOut->at(0).fwBuffer.getData();
    ASSERT_EQ(getSpacePacketApid(frameData, 0), static_cast<U16>(ComCfg::Apid::FW_PACKET_TELEM));

    returnBuffer(0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);

    const U8* secondFrame = this->fromPortHistory_drvSendOut->at(1).fwBuffer.getData();
    ASSERT_EQ(getSpacePacketApid(secondFrame, 0), static_cast<U16>(ComCfg::Apid::FW_PACKET_LOG));
}

void TmFramerTester ::testBufferOverflow() {
    fillActiveBufferToCapacity();
    ASSERT_from_drvSendOut_SIZE(0);

    // No room left at all -- this must overflow and trigger a flush.
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, sizeof(FwPacketDescriptorType));
    ASSERT_from_drvSendOut_SIZE(1);
}

void TmFramerTester ::testDoubleBufferLifecycle() {
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10, 0xAA);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);
    returnBuffer(0);

    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10, 0xBB);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);
    returnBuffer(1);

    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10, 0xCC);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);
}

void TmFramerTester ::testBackpressure() {
    // schedIn_handler refuses to flush the active buffer if doing so would put both buffers
    // in flight, so the only way both buffers become TRANSMITTING without an explicit return
    // in between is via comPacketQueueIn_handler's own overflow-triggered flush, which has no
    // such guard.
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);  // buffer 0 now TRANSMITTING

    fillActiveBufferToCapacity();
    ASSERT_from_drvSendOut_SIZE(1);

    // Overflowing buffer 1 flushes it (buffer 1 -> TRANSMITTING) and re-activates buffer 0,
    // which is still TRANSMITTING -- so this same overflow-triggering packet gets dropped.
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, sizeof(FwPacketDescriptorType));
    ASSERT_from_drvSendOut_SIZE(2);

    // DroppedPackets telemetry is only written from schedIn_handler, not at drop time. It's
    // `update on change`, so the first schedIn call (writing 0) already produced one history
    // entry; this is the second, now that the count has changed to 1.
    invoke_to_schedIn(0, 0);
    ASSERT_TLM_DroppedPackets_SIZE(2);
    ASSERT_TLM_DroppedPackets(1, 1);

    // Free up both buffers and confirm normal operation resumes
    returnBuffer(0);
    returnBuffer(1);
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);
}

void TmFramerTester ::testSchedInFlush() {
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10);
    ASSERT_from_drvSendOut_SIZE(0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    // Nothing accumulated -- schedIn with an idle active buffer is a no-op
    returnBuffer(0);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);
}

void TmFramerTester ::testPerApidSequenceCounts() {
    // TELEM and LOG packets are fanned into the same port but must track independent
    // sequence counts (see nextApidSequenceCount / ApidSequenceSlot).
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10);
    sendPacket(ComCfg::Apid::FW_PACKET_LOG, 10);
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10);
    sendPacket(ComCfg::Apid::FW_PACKET_LOG, 10);
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    const U8* frameData = this->fromPortHistory_drvSendOut->at(0).fwBuffer.getData();
    const FwSizeType packetStride = Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + 10;

    ASSERT_EQ(getSpacePacketApid(frameData, 0), static_cast<U16>(ComCfg::Apid::FW_PACKET_TELEM));
    ASSERT_EQ(getSpacePacketSeqCount(frameData, 0), 0);
    ASSERT_EQ(getSpacePacketApid(frameData, packetStride), static_cast<U16>(ComCfg::Apid::FW_PACKET_LOG));
    ASSERT_EQ(getSpacePacketSeqCount(frameData, packetStride), 0);
    ASSERT_EQ(getSpacePacketApid(frameData, 2 * packetStride), static_cast<U16>(ComCfg::Apid::FW_PACKET_TELEM));
    ASSERT_EQ(getSpacePacketSeqCount(frameData, 2 * packetStride), 1);
    ASSERT_EQ(getSpacePacketApid(frameData, 3 * packetStride), static_cast<U16>(ComCfg::Apid::FW_PACKET_LOG));
    ASSERT_EQ(getSpacePacketSeqCount(frameData, 3 * packetStride), 1);
}

void TmFramerTester ::testFrameCountWrapAround() {
    // Master/Virtual Frame Count are U8 and must wrap mod-256. Flush 257 frames (>255) and
    // confirm the counts wrap back around rather than overflowing/asserting. Checks the
    // component's own counters directly (via friendship) rather than keeping 257 frames of
    // port history alive.
    ASSERT_EQ(this->component.m_masterFrameCount, 0);
    for (U32 i = 0; i < 257; i++) {
        sendPacket(ComCfg::Apid::FW_PACKET_TELEM, 10);
        invoke_to_schedIn(0, 0);
        returnBuffer(0);
        this->fromPortHistory_drvSendOut->clear();
    }
    ASSERT_EQ(this->component.m_masterFrameCount, 1);  // wrapped: 257 mod 256 == 1
    ASSERT_EQ(this->component.m_virtualFrameCount, 1);
}

void TmFramerTester ::testUnexpectedBufferReturn() {
    U8 bufferData[10];
    Fw::Buffer foreignBuffer(bufferData, sizeof(bufferData));
    ASSERT_DEATH_IF_SUPPORTED(this->invoke_to_drvReturnIn(0, foreignBuffer, Drv::ByteStreamStatus::OP_OK),
                               "Assert:");
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

Fw::ComBuffer TmFramerTester ::makeComBuffer(ComCfg::Apid apid, FwSizeType size, U8 fillValue) {
    // `size` is the on-wire ComBuffer size (matching comPacketQueueIn_handler's spaceNeeded =
    // SpacePacketHeader::SERIALIZED_SIZE + data.getSize()), so the fill loop must account for
    // the leading FwPacketDescriptorType this writes first.
    FW_ASSERT(size >= sizeof(FwPacketDescriptorType), static_cast<FwAssertArgType>(size));
    Fw::ComBuffer comBuf;
    Fw::SerializeStatus status = comBuf.serializeFrom(static_cast<FwPacketDescriptorType>(apid));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    FwSizeType fillBytes = size - sizeof(FwPacketDescriptorType);
    for (FwSizeType i = 0; i < fillBytes; i++) {
        status = comBuf.serializeFrom(&fillValue, sizeof(fillValue), Fw::Serialization::OMIT_LENGTH);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }
    return comBuf;
}

void TmFramerTester ::sendPacket(ComCfg::Apid apid, FwSizeType size, U8 fillValue) {
    Fw::ComBuffer comBuf = makeComBuffer(apid, size, fillValue);
    this->invoke_to_comPacketQueueIn(0, comBuf, 0);
}

void TmFramerTester ::fillActiveBufferToCapacity() {
    const FwSizeType dataFieldCapacity =
        static_cast<FwSizeType>(ComCfg::TmFrameFixedSize) -
        (Svc::Ccsds::TMHeader::SERIALIZED_SIZE + Svc::Ccsds::TMTrailer::SERIALIZED_SIZE);
    const FwSizeType maxPacketWireSize = Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + FW_COM_BUFFER_MAX_SIZE;

    // Max-size packets, then a final packet sized to land exactly at dataFieldCapacity
    // (remainder == 0 is not a dead zone -- see comPacketQueueIn_handler's own invariant
    // comment), leaving zero room for anything further.
    FwSizeType used = 0;
    while (dataFieldCapacity - used > maxPacketWireSize) {
        sendPacket(ComCfg::Apid::FW_PACKET_TELEM, FW_COM_BUFFER_MAX_SIZE);
        used += maxPacketWireSize;
    }
    FwSizeType topOffComBufferSize = dataFieldCapacity - used - Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE;
    sendPacket(ComCfg::Apid::FW_PACKET_TELEM, topOffComBufferSize);
}

void TmFramerTester ::returnBuffer(FwIndexType historyIndex) {
    ASSERT_LT(historyIndex, this->fromPortHistory_drvSendOut->size());
    const Fw::Buffer& sentConst = this->fromPortHistory_drvSendOut->at(historyIndex).fwBuffer;
    Fw::Buffer sent(sentConst.getData(), sentConst.getSize());
    this->invoke_to_drvReturnIn(0, sent, Drv::ByteStreamStatus::OP_OK);
}

U16 TmFramerTester ::getFrameScId(const U8* frameData) {
    U16 globalVcId = static_cast<U16>((frameData[0] << 8) | frameData[1]);
    return (globalVcId >> Svc::Ccsds::TMSubfields::spacecraftIdOffset) & 0x3FF;
}
U8 TmFramerTester ::getFrameVcId(const U8* frameData) {
    U16 globalVcId = static_cast<U16>((frameData[0] << 8) | frameData[1]);
    return static_cast<U8>((globalVcId >> Svc::Ccsds::TMSubfields::virtualChannelIdOffset) & 0x7);
}
U8 TmFramerTester ::getFrameMcCount(const U8* frameData) {
    return frameData[2];
}
U8 TmFramerTester ::getFrameVcCount(const U8* frameData) {
    return frameData[3];
}

U16 TmFramerTester ::getSpacePacketApid(const U8* frameData, FwSizeType dataFieldOffset) {
    const U8* spOffset = frameData + Svc::Ccsds::TMHeader::SERIALIZED_SIZE + dataFieldOffset;
    U16 packetIdentification = static_cast<U16>((spOffset[0] << 8) | spOffset[1]);
    return packetIdentification & Svc::Ccsds::SpacePacketSubfields::ApidMask;
}
U16 TmFramerTester ::getSpacePacketSeqCount(const U8* frameData, FwSizeType dataFieldOffset) {
    const U8* spOffset = frameData + Svc::Ccsds::TMHeader::SERIALIZED_SIZE + dataFieldOffset;
    U16 packetSequenceControl = static_cast<U16>((spOffset[2] << 8) | spOffset[3]);
    return packetSequenceControl & Svc::Ccsds::SpacePacketSubfields::SeqCountMask;
}

}  // namespace Samd21

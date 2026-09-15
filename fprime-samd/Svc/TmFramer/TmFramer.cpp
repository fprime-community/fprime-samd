// ======================================================================
// \title  TmFramer.cpp
// \brief  cpp file for CCSDS TM Transfer Frame downlink framer implementation class
// ======================================================================

#include "fprime-samd/Svc/TmFramer/TmFramer.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Types/Assert.hpp"
#include "Svc/Ccsds/Utils/CRC16.hpp"
#include "samd-config/FramerConfig.hpp"

namespace Samd21 {

// Out-of-class definition required pre-C++17 for any ODR-use of a static constexpr member
constexpr U8 TmFramer::IDLE_DATA_PATTERN;

static_assert(ComCfg::TmFrameFixedSize >
                  Svc::Ccsds::TMHeader::SERIALIZED_SIZE + Svc::Ccsds::TMTrailer::SERIALIZED_SIZE,
              "ComCfg::TmFrameFixedSize too small to hold TMHeader + TMTrailer");

static constexpr FwSizeType TM_DATA_FIELD_CAPACITY =
    ComCfg::TmFrameFixedSize - (Svc::Ccsds::TMHeader::SERIALIZED_SIZE + Svc::Ccsds::TMTrailer::SERIALIZED_SIZE);


static constexpr FwSizeType IDLE_SPACE_PACKET_MIN_SIZE = Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + 1;
static constexpr FwSizeType MAX_SINGLE_SPACE_PACKET_SIZE =
    Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + FW_COM_BUFFER_MAX_SIZE;
static_assert(TM_DATA_FIELD_CAPACITY < MAX_SINGLE_SPACE_PACKET_SIZE ||
                  (TM_DATA_FIELD_CAPACITY - MAX_SINGLE_SPACE_PACKET_SIZE) == 0 ||
                  (TM_DATA_FIELD_CAPACITY - MAX_SINGLE_SPACE_PACKET_SIZE) >= IDLE_SPACE_PACKET_MIN_SIZE,
              "FW_COM_BUFFER_MAX_SIZE vs ComCfg::TmFrameFixedSize can leave an unfillable "
              "single-packet remainder on an empty TM frame buffer");

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

TmFramer ::TmFramer(const char* const compName)
    : TmFramerComponentBase(compName),
      m_driverConnected(false),
      m_activeBufferIdx(0),
      m_droppedPackets(0),
      m_masterFrameCount(0),
      m_virtualFrameCount(0) {
    for (FwIndexType i = 0; i < 2; i++) {
        m_buffers[i].dataFieldSize = 0;
        m_buffers[i].state = IDLE;
    }
    for (FwIndexType i = 0; i < NUM_APID_SEQUENCE_SLOTS; i++) {
        m_apidSequenceCounts[i] = 0;
    }
}

TmFramer ::~TmFramer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

// Unlike stock Svc::Ccsds::TmFramer (one buffer per frame), this component accumulates
// multiple Space Packets per frame

void TmFramer ::comPacketQueueIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    TxBuffer* activeBuf = &m_buffers[m_activeBufferIdx];

    // Space Packet header (6B) + payload, no idle padding included yet.
    FwSizeType spaceNeeded = Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + data.getSize();

    // If both buffers are in flight (backpressure), drop this packet
    if (activeBuf->state == TRANSMITTING) {
        this->m_droppedPackets++;
        return;
    }

    // Flush first if adding this Space Packet would overflow the data field, OR would fit but
    // leave a remainder too small for closeFrame()'s idle pad (the dead zone described above).
    FwSizeType afterAppend = activeBuf->dataFieldSize + spaceNeeded;
    bool wouldOverflow = afterAppend > TM_DATA_FIELD_CAPACITY;
    bool wouldLeaveDeadZone = false;
    if (!wouldOverflow) {
        FwSizeType remainderAfterAppend = TM_DATA_FIELD_CAPACITY - afterAppend;
        wouldLeaveDeadZone = remainderAfterAppend > 0 && remainderAfterAppend < IDLE_SPACE_PACKET_MIN_SIZE;
    }

    if (wouldOverflow || wouldLeaveDeadZone) {
        flushActiveBuffer();

        // flushActiveBuffer() swapped the active buffer -- re-acquire it
        activeBuf = &m_buffers[m_activeBufferIdx];

        // If both buffers are in flight (backpressure), drop this packet
        if (activeBuf->state == TRANSMITTING) {
            this->m_droppedPackets++;
            return;
        }

        FW_ASSERT(activeBuf->state == IDLE);
        FW_ASSERT(activeBuf->dataFieldSize == 0);
    }

    // A single ComBuffer + Space Packet header must never exceed the entire data field
    // capacity -- otherwise it could never fit in any frame regardless of flushing.
    FW_ASSERT(spaceNeeded <= TM_DATA_FIELD_CAPACITY, static_cast<FwAssertArgType>(spaceNeeded));

    if (activeBuf->state == IDLE) {
        activeBuf->state = ACTIVE;
    }

    // Data field starts after the (not-yet-written) TMHeader; append at that offset.
    U8* dataFieldStart = activeBuf->data + Svc::Ccsds::TMHeader::SERIALIZED_SIZE;
    FwSizeType written = appendSpacePacket(dataFieldStart + activeBuf->dataFieldSize,
                                            TM_DATA_FIELD_CAPACITY - activeBuf->dataFieldSize, data);
    activeBuf->dataFieldSize += written;
    FW_ASSERT(activeBuf->dataFieldSize <= TM_DATA_FIELD_CAPACITY,
              static_cast<FwAssertArgType>(activeBuf->dataFieldSize));

    // dataFieldSize must end up either exactly full or with room for a valid idle pad.
    FwSizeType finalRemainder = TM_DATA_FIELD_CAPACITY - activeBuf->dataFieldSize;
    FW_ASSERT(finalRemainder == 0 || finalRemainder >= IDLE_SPACE_PACKET_MIN_SIZE,
              static_cast<FwAssertArgType>(finalRemainder));
}

ComCfg::Apid TmFramer ::apidForComBuffer(const Fw::ComBuffer& data) const {
    // Every ComBuffer from Fw::TlmPacket/Fw::LogPacket leads with a FwPacketDescriptorType
    // (Fw::ComPacket::serializeBase()) that is bit-identical to the ComCfg::Apid value we
    // want. Peek it via a temporary ExternalSerializeBuffer over the same memory so `data`'s
    // own cursor is untouched -- setBuffLen() marks its bytes readable without copying.
    Fw::ExternalSerializeBuffer peek(const_cast<U8*>(data.getBuffAddr()), data.getSize());
    Fw::SerializeStatus status = peek.setBuffLen(data.getSize());
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    FwPacketDescriptorType descriptor = 0;
    status = peek.deserializeTo(descriptor);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    return static_cast<ComCfg::Apid>(descriptor);
}

U16 TmFramer ::nextApidSequenceCount(ComCfg::Apid apid) {
    ApidSequenceSlot slot;
    switch (apid) {
        case ComCfg::Apid::FW_PACKET_LOG:
            slot = LOG_SLOT;
            break;
        case ComCfg::Apid::FW_PACKET_TELEM:
        default:
            // Any APID this framer doesn't have a dedicated slot for tracks alongside
            // telemetry rather than asserting -- see enum comment in TmFramer.hpp.
            slot = TELEM_SLOT;
            break;
    }

    U16 count = this->m_apidSequenceCounts[slot];
    // Sequence count is 14 bits, mod-16384 per APID (CCSDS 133.0-B-2 4.1.3.4).
    this->m_apidSequenceCounts[slot] =
        static_cast<U16>((count + 1) & Svc::Ccsds::SpacePacketSubfields::SeqCountMask);
    return count;
}

FwSizeType TmFramer ::appendSpacePacket(U8* dest, FwSizeType destCapacity, Fw::ComBuffer& data) {
    FW_ASSERT(destCapacity >= Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + data.getSize());

    Fw::Buffer bufWrapper(dest, destCapacity);
    Fw::ExternalSerializeBuffer serializer(bufWrapper.getData(), bufWrapper.getSize());

    // Space Packet primary header (CCSDS 133.0-B-2): Packet Type = 0 (TM), Secondary Header
    // Flag = 0, Sequence Flags = 0b11 (unsegmented) for every packet this framer builds.
    ComCfg::Apid apid = apidForComBuffer(data);

    Svc::Ccsds::SpacePacketHeader header;
    U16 packetIdentification = static_cast<U16>(apid) & Svc::Ccsds::SpacePacketSubfields::ApidMask;
    header.set_packetIdentification(packetIdentification);

    U16 packetSequenceControl = static_cast<U16>(0x3 << Svc::Ccsds::SpacePacketSubfields::SeqFlagsOffset);
    packetSequenceControl |= (nextApidSequenceCount(apid) & Svc::Ccsds::SpacePacketSubfields::SeqCountMask);
    header.set_packetSequenceControl(packetSequenceControl);

    // Packet Data Length = size of user data field minus one (CCSDS 133.0-B-2 4.1.3.5.3)
    FW_ASSERT(data.getSize() > 0, static_cast<FwAssertArgType>(data.getSize()));
    header.set_packetDataLength(static_cast<U16>(data.getSize() - 1));

    Fw::SerializeStatus status = serializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    status = serializer.serializeFrom(data.getBuffAddr(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status, data.getSize());

    return serializer.getSize();
}

void TmFramer ::drvConnected_handler(FwIndexType portNum) {
    this->m_driverConnected = true;
}

void TmFramer ::drvReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const Drv::ByteStreamStatus& status) {
    for (FwIndexType i = 0; i < 2; i++) {
        if (m_buffers[i].state == TRANSMITTING && fwBuffer.getData() == m_buffers[i].data) {
            m_buffers[i].state = IDLE;
            m_buffers[i].dataFieldSize = 0;
            return;
        }
    }
    FW_ASSERT(false);  // Buffer returned that we didn't send
}

void TmFramer ::closeFrame(U8* frameData, FwSizeType dataFieldUsed) {
    U8* dataField = frameData + Svc::Ccsds::TMHeader::SERIALIZED_SIZE;

    // Pad the remainder of the data field with a CCSDS idle Space Packet
    // (CCSDS 132.0-B-3 4.2.2.5).
    FwSizeType remaining = TM_DATA_FIELD_CAPACITY - dataFieldUsed;
    if (remaining > 0) {
        FW_ASSERT(remaining >= Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE + 1,
                  static_cast<FwAssertArgType>(remaining));

        Fw::Buffer idleWrapper(dataField + dataFieldUsed, remaining);
        Fw::ExternalSerializeBuffer idleSerializer(idleWrapper.getData(), idleWrapper.getSize());

        Svc::Ccsds::SpacePacketHeader idleHeader;
        idleHeader.set_packetIdentification(static_cast<U16>(ComCfg::Apid::SPP_IDLE_PACKET) &
                                             Svc::Ccsds::SpacePacketSubfields::ApidMask);
        idleHeader.set_packetSequenceControl(static_cast<U16>(0x3 << Svc::Ccsds::SpacePacketSubfields::SeqFlagsOffset));
        FwSizeType idleDataLen = remaining - Svc::Ccsds::SpacePacketHeader::SERIALIZED_SIZE;
        idleHeader.set_packetDataLength(static_cast<U16>(idleDataLen - 1));

        Fw::SerializeStatus status = idleSerializer.serializeFrom(idleHeader);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        for (FwSizeType i = 0; i < idleDataLen; i++) {
            status = idleSerializer.serializeFrom(IDLE_DATA_PATTERN);
            FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
        }
    }

    // -----------------------------------------------
    // Header (TM Transfer Frame Primary Header, CCSDS 132.0-B-3 4.1.2)
    // -----------------------------------------------
    Svc::Ccsds::TMHeader header;
    U16 globalVcId = static_cast<U16>(1 << Svc::Ccsds::TMSubfields::virtualChannelIdOffset);  // VCID=1, this project's downlink virtual channel
    globalVcId |= static_cast<U16>(ComCfg::SpacecraftId << Svc::Ccsds::TMSubfields::spacecraftIdOffset);
    globalVcId |= 0x0;  // Operational Control Field: Flag = 0 (no OCF present)

    U16 dataFieldStatus = static_cast<U16>(0x3 << Svc::Ccsds::TMSubfields::segLengthOffset);  // Seg Length Id = 0b11
    // First Header Pointer = 0: the first Space Packet in every frame starts at data-field
    // offset 0 (this framer never spans a packet across frames).

    header.set_globalVcId(globalVcId);
    header.set_masterFrameCount(this->m_masterFrameCount);
    header.set_virtualFrameCount(this->m_virtualFrameCount);
    header.set_dataFieldStatus(dataFieldStatus);

    // Single Master Channel / Virtual Channel in this deployment, so both counters
    // track together.
    this->m_masterFrameCount++;   // U8 wraps mod-256
    this->m_virtualFrameCount++;  // U8 wraps mod-256

    Fw::Buffer headerWrapper(frameData, Svc::Ccsds::TMHeader::SERIALIZED_SIZE);
    Fw::ExternalSerializeBuffer headerSerializer(headerWrapper.getData(), headerWrapper.getSize());
    Fw::SerializeStatus status = headerSerializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // -----------------------------------------------
    // Trailer (FECF, CRC-16/CCITT-FALSE per CCSDS 132.0-B-3 4.1.6)
    // -----------------------------------------------
    U16 crc = Svc::Ccsds::Utils::CRC16::compute(frameData, ComCfg::TmFrameFixedSize - Svc::Ccsds::TMTrailer::SERIALIZED_SIZE);
    Svc::Ccsds::TMTrailer trailer;
    trailer.set_fecf(crc);

    Fw::Buffer trailerWrapper(frameData + ComCfg::TmFrameFixedSize - Svc::Ccsds::TMTrailer::SERIALIZED_SIZE,
                               Svc::Ccsds::TMTrailer::SERIALIZED_SIZE);
    Fw::ExternalSerializeBuffer trailerSerializer(trailerWrapper.getData(), trailerWrapper.getSize());
    status = trailerSerializer.serializeFrom(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
}

void TmFramer ::flushActiveBuffer() {
    FW_ASSERT(this->m_driverConnected);

    TxBuffer& activeBuf = m_buffers[m_activeBufferIdx];

    // Nothing to flush if the buffer hasn't been touched since its last return.
    if (activeBuf.state == IDLE) {
        return;
    }

    FW_ASSERT(activeBuf.state == ACTIVE);
    FW_ASSERT(activeBuf.dataFieldSize > 0);

    closeFrame(activeBuf.data, activeBuf.dataFieldSize);

    activeBuf.state = TRANSMITTING;
    m_activeBufferIdx = (m_activeBufferIdx + 1) % 2;

    Fw::Buffer txBuffer(activeBuf.data, ComCfg::TmFrameFixedSize);
    drvSendOut_out(0, txBuffer);
}

void TmFramer ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_driverConnected) {
        FwIndexType nextBufferIdx = (m_activeBufferIdx + 1) % 2;
        TxBuffer& nextBuf = m_buffers[nextBufferIdx];

        this->tlmWrite_DroppedPackets(this->m_droppedPackets);

        if (nextBuf.state == TRANSMITTING) {
            // Can't flush because both buffers would be in flight
            return;
        }

        flushActiveBuffer();
    }
}

}  // namespace Samd21

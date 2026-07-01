// ======================================================================
// \title  Framer.cpp
// \author tumbar
// \brief  cpp file for Framer component implementation class
// ======================================================================

#include "fprime-samd/Svc/Framer/Framer.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"
#include "Utils/Hash/Hash.hpp"
#include "fprime-samd/config/FramerConfig.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Framer ::Framer(const char* const compName)
    : FramerComponentBase(compName), m_driverConnected(false), m_activeBufferIdx(0) {
    // Initialize both buffers to IDLE state with space reserved for header
    for (FwIndexType i = 0; i < 2; i++) {
        m_buffers[i].size = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE;
        m_buffers[i].state = IDLE;
    }
}

Framer ::~Framer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Framer ::comPacketQueueIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    TxBuffer& activeBuf = m_buffers[m_activeBufferIdx];

    // Calculate space needed: ComBuffer data + trailer (header already reserved)
    FwSizeType spaceNeeded = data.getSize() + Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;

    // If both buffers are in flight (backpressure), drop this packet
    if (activeBuf.state == TRANSMITTING) {
        // TODO: Add telemetry to track dropped packets due to backpressure
        return;
    }

    // If adding this ComBuffer would overflow, flush first
    if (activeBuf.size + spaceNeeded > FramerConfig::FRAMER_TX_BUFFER_SIZE) {
        flushActiveBuffer();

        // If both buffers are in flight (backpressure), drop this packet
        if (activeBuf.state == TRANSMITTING) {
            // TODO: Add telemetry to track dropped packets due to backpressure
            return;
        }

        // After flush, active buffer should be IDLE with header space reserved
        FW_ASSERT(activeBuf.state == IDLE);
        FW_ASSERT(activeBuf.size == Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE);
    }

    // Mark buffer as active if it's idle
    if (activeBuf.state == IDLE) {
        activeBuf.state = ACTIVE;
    }

    // Append ComBuffer data directly to the buffer (after existing data)
    Fw::Buffer bufWrapper(activeBuf.data + activeBuf.size, FramerConfig::FRAMER_TX_BUFFER_SIZE - activeBuf.size);
    Fw::ExternalSerializeBuffer serializer(bufWrapper.getData(), bufWrapper.getSize());

    Fw::SerializeStatus status =
        serializer.serializeFrom(data.getBuffAddr(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status, data.getSize(), portNum);

    // Update buffer size to include the ComBuffer data
    activeBuf.size += serializer.getSize();
    FW_ASSERT(activeBuf.size <= FramerConfig::FRAMER_TX_BUFFER_SIZE);
}

void Framer ::comPacketSyncIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    Svc::FprimeProtocol::FrameHeader header;
    Svc::FprimeProtocol::FrameTrailer trailer;

    // Full size of the frame will be size of header + data + trailer
    FwSizeType frameSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + data.getSize() +
                           Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    FW_ASSERT(data.getSize() <= std::numeric_limits<Svc::FprimeProtocol::TokenType>::max(),
              static_cast<FwAssertArgType>(frameSize));
    FW_ASSERT(frameSize <= std::numeric_limits<Fw::Buffer::SizeType>::max(), static_cast<FwAssertArgType>(frameSize));

    Fw::Buffer pkt_buf(this->m_buffers[0].data, FramerConfig::FRAMER_TX_BUFFER_SIZE);
    auto pkt = pkt_buf.getSerializer();

    Fw::SerializeStatus status;

    // Serialize the header
    // 0xDEADBEEF is already set as the default value for the header startWord field in the FPP type definition
    header.set_lengthField(static_cast<Svc::FprimeProtocol::TokenType>(data.getSize()));
    status = pkt.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Serialize the data
    status = pkt.serializeFrom(data.getBuffAddr(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status, data.getSize());

    // Serialize the trailer (with CRC computation)
    Utils::HashBuffer hashBuffer;
    Utils::Hash::hash(pkt.getBuffAddr(), frameSize - HASH_DIGEST_LENGTH, hashBuffer);
    trailer.set_crcField(hashBuffer.asBigEndianU32());
    status = pkt.serializeFrom(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    Fw::Buffer buf(pkt.getBuffAddr(), pkt.getSize());
    auto sendStatus = drvSendSyncOut_out(0, buf);
    FW_ASSERT(sendStatus == Drv::ByteStreamStatus::OP_OK, sendStatus);
}

void Framer ::sendFatalPacket(Fw::ComBuffer& data) {
    this->comPacketSyncIn_handler(0, data, 0);
}

void Framer ::drvConnected_handler(FwIndexType portNum) {
    this->m_driverConnected = true;
}

void Framer ::drvReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const Drv::ByteStreamStatus& status) {
    // Find which buffer was returned
    for (FwIndexType i = 0; i < 2; i++) {
        if (m_buffers[i].state == TRANSMITTING && fwBuffer.getData() == m_buffers[i].data) {
            // Mark buffer as idle and ready for reuse, reset size to header-only
            m_buffers[i].state = IDLE;
            m_buffers[i].size = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE;
            return;
        }
    }
    FW_ASSERT(false);  // Buffer returned that we didn't send
}

void Framer ::flushActiveBuffer() {
    FW_ASSERT(this->m_driverConnected);

    TxBuffer& activeBuf = m_buffers[m_activeBufferIdx];

    // Nothing to flush if buffer is idle or contains only header
    if (activeBuf.state == IDLE || activeBuf.size <= Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE) {
        return;
    }

    // Buffer must not already be in flight
    FW_ASSERT(activeBuf.state == ACTIVE);

    // Finalize the frame by writing header and trailer

    // 1. Update header with total payload length (size - header)
    FwSizeType payloadSize = activeBuf.size - Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE;
    FW_ASSERT(payloadSize <= std::numeric_limits<Svc::FprimeProtocol::TokenType>::max(),
              static_cast<FwAssertArgType>(payloadSize));

    Svc::FprimeProtocol::FrameHeader header;
    header.set_lengthField(static_cast<Svc::FprimeProtocol::TokenType>(payloadSize));

    Fw::Buffer headerBuf(activeBuf.data, Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE);
    Fw::ExternalSerializeBuffer headerSerializer(headerBuf.getData(), headerBuf.getSize());
    Fw::SerializeStatus status = headerSerializer.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // 2. Compute CRC over [header + all ComBuffers]
    Utils::HashBuffer hashBuffer;
    Utils::Hash::hash(activeBuf.data, activeBuf.size, hashBuffer);

    // 3. Write trailer with CRC
    Svc::FprimeProtocol::FrameTrailer trailer;
    trailer.set_crcField(hashBuffer.asBigEndianU32());

    Fw::Buffer trailerBuf(activeBuf.data + activeBuf.size, Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE);
    Fw::ExternalSerializeBuffer trailerSerializer(trailerBuf.getData(), trailerBuf.getSize());
    status = trailerSerializer.serializeFrom(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Update size to include trailer
    FwSizeType totalFrameSize = activeBuf.size + Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;

    // Mark buffer as in-flight
    activeBuf.state = TRANSMITTING;

    // Send the complete frame to the driver
    Fw::Buffer txBuffer(activeBuf.data, totalFrameSize);
    drvSendOut_out(0, txBuffer);

    // Try to switch to the other buffer
    FwIndexType nextBufferIdx = (m_activeBufferIdx + 1) % 2;
    TxBuffer& nextBuf = m_buffers[nextBufferIdx];

    if (nextBuf.state == TRANSMITTING) {
        // Both buffers are in flight - backpressure situation
        // Don't switch buffers; incoming packets will be dropped until a buffer returns
        // TODO: Add telemetry to track backpressure events
        return;
    }

    // Switch to the next buffer
    m_activeBufferIdx = nextBufferIdx;
}

void Framer ::schedIn_handler(FwIndexType portNum, U32 context) {
    // Only flush if the other buffer is not transmitting
    FwIndexType nextBufferIdx = (m_activeBufferIdx + 1) % 2;
    TxBuffer& nextBuf = m_buffers[nextBufferIdx];

    if (nextBuf.state == TRANSMITTING) {
        // Can't flush because both buffers would be in flight
        // Wait for next schedule cycle
        return;
    }

    flushActiveBuffer();
}

}  // namespace Samd21

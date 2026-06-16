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

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Framer ::Framer(const char* const compName) : FramerComponentBase(compName), m_driverConnected(false) {}

Framer ::~Framer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void Framer ::comPacketQueueIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    FW_ASSERT(m_driverConnected);

    Svc::FprimeProtocol::FrameHeader header;
    Svc::FprimeProtocol::FrameTrailer trailer;

    // Full size of the frame will be size of header + data + trailer
    FwSizeType frameSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + data.getSize() +
                           Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    FW_ASSERT(data.getSize() <= std::numeric_limits<Svc::FprimeProtocol::TokenType>::max(),
              static_cast<FwAssertArgType>(frameSize));
    FW_ASSERT(frameSize <= std::numeric_limits<Fw::Buffer::SizeType>::max(), static_cast<FwAssertArgType>(frameSize));

    static U8 frame[64 + 12];
    Fw::Buffer pkt_buf(frame, 64 + 12);
    auto pkt = pkt_buf.getSerializer();

    Fw::SerializeStatus status;

    // Serialize the header
    // 0xDEADBEEF is already set as the default value for the header startWord field in the FPP type definition
    header.set_lengthField(static_cast<Svc::FprimeProtocol::TokenType>(data.getSize()));
    status = pkt.serializeFrom(header);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Serialize the data
    status = pkt.serializeFrom(data.getBuffAddr(), data.getSize(), Fw::Serialization::OMIT_LENGTH);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status, data.getSize(), portNum);

    // Serialize the trailer (with CRC computation)
    Utils::HashBuffer hashBuffer;
    Utils::Hash::hash(pkt.getBuffAddr(), frameSize - HASH_DIGEST_LENGTH, hashBuffer);
    trailer.set_crcField(hashBuffer.asBigEndianU32());
    status = pkt.serializeFrom(trailer);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    Fw::Buffer buf(pkt.getBuffAddr(), pkt.getSize());
    auto sendStatus = drvSendOut_out(0, buf);
    FW_ASSERT(sendStatus == Drv::ByteStreamStatus::OP_OK, sendStatus);
}

void Framer ::sendFatalPacket(Fw::ComBuffer& data) {
    this->comPacketQueueIn_handler(0, data, 0);
}

void Framer ::drvConnected_handler(FwIndexType portNum) {
    m_driverConnected = true;
}

}  // namespace Samd21

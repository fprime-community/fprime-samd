// ======================================================================
// \title  SyncFramer.cpp
// \author tumbar
// \brief  cpp file for SyncFramer component implementation class
// ======================================================================

#include "fprime-samd/Svc/SyncFramer/SyncFramer.hpp"
#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"
#include "Utils/Hash/Crc32/Crc32.hpp"
#include "Utils/Hash/Hash.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SyncFramer::SyncFramer(const char* const compName) : SyncFramerComponentBase(compName) {}

SyncFramer::~SyncFramer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void SyncFramer::comPacketIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    Svc::FprimeProtocol::FrameHeader header;
    Svc::FprimeProtocol::FrameTrailer trailer;

    static_assert(static_cast<FwSizeType>(Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE) >=
                      static_cast<FwSizeType>(Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE),
                  "sizeof(header) should be >= sizeof(trailer) so that we can reuse it's buffer");
    U8 frameScratch[Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE];
    Fw::Buffer headerBuf(frameScratch, Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE);
    auto headerSer = headerBuf.getSerializer();

    // Serialize the header
    // 0xDEADBEEF is already set as the default value for the header startWord field in the FPP type definition
    header.set_lengthField(static_cast<Svc::FprimeProtocol::TokenType>(data.getSize()));
    (void)headerSer.serializeFrom(header);

    // Transmit the header
    (void)this->drvSendOut_out(0, headerBuf);

    // Transmit the data
    auto dataBuf = Fw::Buffer(data.getBuffAddr(), data.getSize());
    (void)this->drvSendOut_out(0, dataBuf);

    // Compute the CRC hash
    Utils::Hash crcHash;
    crcHash.update(frameScratch, Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE);
    crcHash.update(data.getBuffAddr(), data.getSize());

    U32 crcValue;
    crcHash.finalize(crcValue);
    trailer.set_crcField(crcValue);

    Fw::Buffer trailerBuf(frameScratch, HASH_DIGEST_LENGTH);
    auto trailerSer = trailerBuf.getSerializer();
    (void)trailerSer.serializeFrom(trailer);

    // Transmit the CRC
    (void)this->drvSendOut_out(0, trailerBuf);
}

void SyncFramer::drvConnected_handler(FwIndexType portNum) {}

}  // namespace Samd21

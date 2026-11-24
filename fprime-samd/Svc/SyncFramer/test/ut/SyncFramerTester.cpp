// ======================================================================
// \title  SyncFramerTester.cpp
// \author tumbar
// \brief  cpp file for SyncFramer component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/SyncFramer/test/ut/SyncFramerTester.hpp"
#include <cstring>
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"
#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"
#include "Svc/FrameAccumulator/FrameDetector/FprimeFrameDetector.hpp"
#include "Utils/Hash/Hash.hpp"
#include "Utils/Types/CircularBuffer.hpp"

namespace Samd21 {

constexpr FwSizeType SyncFramerTester::FRAME_OVERHEAD;
constexpr FwSizeType SyncFramerTester::STREAM_CAPACITY;

static_assert(SyncFramerTester::FRAME_OVERHEAD == Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE +
                                                      Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE,
              "FRAME_OVERHEAD must match the F Prime protocol header + trailer");

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SyncFramerTester::SyncFramerTester()
    : SyncFramerGTestBase("SyncFramerTester", SyncFramerTester::MAX_HISTORY_SIZE),
      component("SyncFramer"),
      m_stream(),
      m_streamSize(0),
      m_sendStatus(Drv::ByteStreamStatus::OP_OK) {
    this->initComponents();
    this->connectPorts();
}

SyncFramerTester::~SyncFramerTester() {}

// ----------------------------------------------------------------------
// Handlers for typed from ports
// ----------------------------------------------------------------------

Drv::ByteStreamStatus SyncFramerTester::from_drvSendOut_handler(FwIndexType portNum, Fw::Buffer& sendBuffer) {
    this->pushFromPortEntry_drvSendOut(sendBuffer);
    const FwSizeType size = sendBuffer.getSize();
    EXPECT_LE(this->m_streamSize + size, STREAM_CAPACITY);
    if (this->m_streamSize + size <= STREAM_CAPACITY && size > 0) {
        (void)::memcpy(&this->m_stream[this->m_streamSize], sendBuffer.getData(), static_cast<size_t>(size));
        this->m_streamSize += size;
    }
    return this->m_sendStatus;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void SyncFramerTester::resetTest() {
    this->clearHistory();
    this->m_streamSize = 0;
}

void SyncFramerTester::fillRandom(Fw::ComBuffer& packet, FwSizeType size) {
    U8 bytes[FW_COM_BUFFER_MAX_SIZE];
    ASSERT_LE(size, sizeof(bytes));
    for (FwSizeType i = 0; i < size; i++) {
        bytes[i] = static_cast<U8>(STest::Pick::lowerUpper(0, 0xFF));
    }
    packet.resetSer();
    ASSERT_EQ(packet.serializeFrom(bytes, size, Fw::Serialization::OMIT_LENGTH), Fw::FW_SERIALIZE_OK);
}

void SyncFramerTester::sendPacket(Fw::ComBuffer& packet) {
    this->invoke_to_comPacketIn(0, packet, STest::Pick::any());
}

U32 SyncFramerTester::streamU32(FwSizeType offset) const {
    return (static_cast<U32>(this->m_stream[offset]) << 24) | (static_cast<U32>(this->m_stream[offset + 1]) << 16) |
           (static_cast<U32>(this->m_stream[offset + 2]) << 8) | static_cast<U32>(this->m_stream[offset + 3]);
}

FwSizeType SyncFramerTester::assertFrame(FwSizeType offset, const Fw::ComBuffer& packet) {
    const FwSizeType size = packet.getSize();
    const FwSizeType headerSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE;
    EXPECT_LE(offset + headerSize + size + Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE, this->m_streamSize);

    // Header
    const Svc::FprimeProtocol::FrameHeader defaultHeader;
    EXPECT_EQ(this->streamU32(offset), defaultHeader.get_startWord());
    EXPECT_EQ(this->streamU32(offset + 4), static_cast<U32>(size));

    // Payload
    EXPECT_EQ(::memcmp(&this->m_stream[offset + headerSize], packet.getBuffAddr(), static_cast<size_t>(size)), 0);

    // Trailer
    U32 crc = 0;
    Utils::Hash hash;
    hash.update(&this->m_stream[offset], headerSize + size);
    hash.finalize(crc);
    EXPECT_EQ(this->streamU32(offset + headerSize + size), crc);

    return offset + headerSize + size + Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SyncFramerTester::testFrameLayout() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-001: SyncFramer shall send each packet received on comPacketIn as one F Prime "
        "protocol frame: a header carrying the 0xDEADBEEF start word and the packet length, the packet, and "
        "a trailer carrying the CRC32 of the header and packet");

    Fw::ComBuffer packet;
    fillRandom(packet, STest::Pick::lowerUpper(1, FW_COM_BUFFER_MAX_SIZE));
    this->sendPacket(packet);

    ASSERT_EQ(this->m_streamSize, packet.getSize() + FRAME_OVERHEAD);
    ASSERT_EQ(this->assertFrame(0, packet), this->m_streamSize);
}

void SyncFramerTester::testKnownCrc() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-001: SyncFramer shall send each packet received on comPacketIn as one F Prime "
        "protocol frame: a header carrying the 0xDEADBEEF start word and the packet length, the packet, and "
        "a trailer carrying the CRC32 of the header and packet");
    COMMENT("Frame bytes match an independently computed frame (CRC-32/IEEE 802.3, as zlib.crc32)");

    const U8 payload[] = {0x01, 0x02, 0x03, 0x04};
    const U8 expected[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x04,
                           0x01, 0x02, 0x03, 0x04, 0xE3, 0xF9, 0xA0, 0x73};

    Fw::ComBuffer packet(payload, sizeof(payload));
    this->sendPacket(packet);

    ASSERT_EQ(this->m_streamSize, sizeof(expected));
    for (FwSizeType i = 0; i < sizeof(expected); i++) {
        ASSERT_EQ(this->m_stream[i], expected[i]) << "at index " << i;
    }
}

void SyncFramerTester::testGroundDeframes() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-002: Frames sent by SyncFramer shall be accepted by the F Prime frame detector "
        "used to deframe the stream on the ground");

    U8 storage[STREAM_CAPACITY];
    for (U32 iter = 0; iter < 100; iter++) {
        this->resetTest();
        Fw::ComBuffer packet;
        fillRandom(packet, STest::Pick::lowerUpper(0, FW_COM_BUFFER_MAX_SIZE));
        this->sendPacket(packet);

        Types::CircularBuffer circular(storage, sizeof(storage));
        ASSERT_EQ(circular.serialize(this->m_stream, this->m_streamSize), Fw::FW_SERIALIZE_OK);

        Svc::FrameDetectors::FprimeFrameDetector detector;
        FwSizeType frameSize = 0;
        ASSERT_EQ(detector.detect(circular, frameSize), Svc::FrameDetector::Status::FRAME_DETECTED)
            << "packet size " << packet.getSize();
        ASSERT_EQ(frameSize, packet.getSize() + FRAME_OVERHEAD);

        // A corrupted byte anywhere in the frame is rejected
        const FwSizeType corrupt = STest::Pick::lowerUpper(0, static_cast<U32>(this->m_streamSize - 1));
        this->m_stream[corrupt] ^= static_cast<U8>(1U << STest::Pick::lowerUpper(0, 7));
        Types::CircularBuffer corrupted(storage, sizeof(storage));
        ASSERT_EQ(corrupted.serialize(this->m_stream, this->m_streamSize), Fw::FW_SERIALIZE_OK);
        ASSERT_NE(detector.detect(corrupted, frameSize), Svc::FrameDetector::Status::FRAME_DETECTED)
            << "corrupted index " << corrupt;
    }
}

void SyncFramerTester::testPacketSizes() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-003: SyncFramer shall frame packets of every size from 0 to FW_COM_BUFFER_MAX_SIZE "
        "bytes");

    for (FwSizeType size = 0; size <= FW_COM_BUFFER_MAX_SIZE; size++) {
        this->resetTest();
        Fw::ComBuffer packet;
        fillRandom(packet, size);
        this->sendPacket(packet);

        ASSERT_EQ(this->m_streamSize, size + FRAME_OVERHEAD) << "packet size " << size;
        ASSERT_EQ(this->assertFrame(0, packet), this->m_streamSize) << "packet size " << size;
    }
}

void SyncFramerTester::testBackToBackFrames() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-004: SyncFramer shall send the whole frame on drvSendOut before comPacketIn "
        "returns");

    constexpr FwSizeType NUM_PACKETS = 4;
    Fw::ComBuffer packets[NUM_PACKETS];
    for (FwSizeType i = 0; i < NUM_PACKETS; i++) {
        fillRandom(packets[i], STest::Pick::lowerUpper(0, FW_COM_BUFFER_MAX_SIZE));
        const FwSizeType before = this->m_streamSize;
        this->sendPacket(packets[i]);
        ASSERT_EQ(this->m_streamSize, before + packets[i].getSize() + FRAME_OVERHEAD);
    }

    // Each frame follows the previous one with nothing in between
    FwSizeType offset = 0;
    for (FwSizeType i = 0; i < NUM_PACKETS; i++) {
        offset = this->assertFrame(offset, packets[i]);
    }
    ASSERT_EQ(offset, this->m_streamSize);
}

void SyncFramerTester::testDriverErrorIgnored() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-005: SyncFramer shall send the whole frame regardless of the status drvSendOut "
        "returns, without asserting");

    const Drv::ByteStreamStatus::T statuses[] = {Drv::ByteStreamStatus::SEND_RETRY, Drv::ByteStreamStatus::OTHER_ERROR};
    for (const Drv::ByteStreamStatus::T status : statuses) {
        this->resetTest();
        this->m_sendStatus = status;

        Fw::ComBuffer packet;
        fillRandom(packet, STest::Pick::lowerUpper(0, FW_COM_BUFFER_MAX_SIZE));
        this->sendPacket(packet);

        ASSERT_from_drvSendOut_SIZE(3);
        ASSERT_EQ(this->assertFrame(0, packet), this->m_streamSize);
    }
}

void SyncFramerTester::testDrvConnectedNoOp() {
    REQUIREMENT(
        "SAMD21-SYNCFRAMER-006: SyncFramer shall frame packets without waiting for drvConnected, and "
        "drvConnected shall have no effect");

    // Framing before any drvConnected
    Fw::ComBuffer packet;
    fillRandom(packet, STest::Pick::lowerUpper(0, FW_COM_BUFFER_MAX_SIZE));
    this->sendPacket(packet);
    ASSERT_EQ(this->assertFrame(0, packet), this->m_streamSize);

    // drvConnected sends nothing and does not change framing
    this->resetTest();
    this->invoke_to_drvConnected(0);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_EQ(this->m_streamSize, 0U);

    this->sendPacket(packet);
    ASSERT_EQ(this->assertFrame(0, packet), this->m_streamSize);
}

}  // namespace Samd21

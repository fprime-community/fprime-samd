// ======================================================================
// \title  FramPeekPokeTester.cpp
// \author tumbar
// \brief  cpp file for FramPeekPoke component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/FramPeekPoke/test/ut/FramPeekPokeTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"

#include <cstring>

namespace Samd21 {

// Definitions of the ODR-used class constants (C++14)
constexpr FwSizeType FramPeekPokeTester::FRAM_SIZE;

namespace {
const FramWordWidth WIDTHS[] = {FramWordWidth::U8_WORD, FramWordWidth::U16_WORD, FramWordWidth::U32_WORD};
}  // namespace

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FramPeekPokeTester::FramPeekPokeTester()
    : FramPeekPokeGTestBase("FramPeekPokeTester", FramPeekPokeTester::MAX_HISTORY_SIZE),
      component("FramPeekPoke"),
      m_fram(),
      m_pending(false),
      m_pendingOp(Fram::Op::READ),
      m_pendingOffset(0),
      m_pendingBuffer(),
      m_reject(Fram::Status::ACCEPTED),
      m_completeInCall(false) {
    this->initComponents();
    this->connectPorts();
    for (FwSizeType i = 0; i < FRAM_SIZE; i++) {
        this->m_fram[i] = static_cast<U8>(i);
    }
}

FramPeekPokeTester::~FramPeekPokeTester() {}

// ----------------------------------------------------------------------
// Simulated F-RAM
// ----------------------------------------------------------------------

Fram::Status FramPeekPokeTester::from_framRead_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->pushFromPortEntry_framRead(offset, buffer);
    return this->framRequest(Fram::Op::READ, offset, buffer);
}

Fram::Status FramPeekPokeTester::from_framWrite_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->pushFromPortEntry_framWrite(offset, buffer);
    return this->framRequest(Fram::Op::WRITE, offset, buffer);
}

Fram::Status FramPeekPokeTester::framRequest(Fram::Op op, U32 offset, Fw::Buffer& buffer) {
    EXPECT_FALSE(this->m_pending);
    if (this->m_reject != Fram::Status::ACCEPTED) {
        const Fram::Status status = this->m_reject;
        this->m_reject = Fram::Status::ACCEPTED;
        return status;
    }
    EXPECT_LE(offset + buffer.getSize(), FRAM_SIZE);
    if (op == Fram::Op::READ) {
        (void)::memcpy(buffer.getData(), &this->m_fram[offset], buffer.getSize());
    } else {
        (void)::memcpy(&this->m_fram[offset], buffer.getData(), buffer.getSize());
    }
    this->m_pending = true;
    this->m_pendingOp = op;
    this->m_pendingOffset = offset;
    this->m_pendingBuffer = buffer;
    if (this->m_completeInCall) {
        this->complete();
    }
    return Fram::Status::ACCEPTED;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void FramPeekPokeTester::sendPeek(U32 cmdSeq, U32 offset, FramWordWidth width) {
    this->clearHistory();
    this->sendCmd_PEEK(TEST_INSTANCE_ID, cmdSeq, offset, width);
}

void FramPeekPokeTester::sendPoke(U32 cmdSeq, U32 offset, FramWordWidth width, U32 value) {
    this->clearHistory();
    this->sendCmd_POKE(TEST_INSTANCE_ID, cmdSeq, offset, width, value);
}

void FramPeekPokeTester::complete(Fram::Status status) {
    ASSERT_TRUE(this->m_pending);
    this->m_pending = false;
    if (this->m_pendingOp == Fram::Op::READ) {
        this->invoke_to_framReadComplete(0, this->m_pendingOffset, this->m_pendingBuffer, status);
    } else {
        this->invoke_to_framWriteComplete(0, this->m_pendingOffset, this->m_pendingBuffer, status);
    }
}

void FramPeekPokeTester::assertResponse(FwOpcodeType opcode, U32 cmdSeq, Fw::CmdResponse response) {
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, opcode, cmdSeq, response);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FramPeekPokeTester::testPeek() {
    for (const FramWordWidth width : WIDTHS) {
        const FwSizeType size = static_cast<FwSizeType>(width.e);
        const U32 offset = static_cast<U32>(STest::Pick::lowerUpper(0, FRAM_SIZE - size));
        const U32 cmdSeq = STest::Pick::any();
        this->sendPeek(cmdSeq, offset, width);
        // Asynchronous: the command completes with the read
        ASSERT_CMD_RESPONSE_SIZE(0);
        ASSERT_from_framRead_SIZE(1);
        ASSERT_EQ(this->fromPortHistory_framRead->at(0).offset, offset);
        ASSERT_EQ(this->fromPortHistory_framRead->at(0).buffer.getSize(), size);
        this->complete();

        U32 expected = 0;
        for (FwSizeType i = 0; i < size; i++) {
            expected = (expected << 8) | this->m_fram[offset + i];
        }
        ASSERT_EVENTS_Peeked_SIZE(1);
        ASSERT_EVENTS_Peeked(0, offset, width, expected);
        this->assertResponse(FramPeekPokeComponentBase::OPCODE_PEEK, cmdSeq, Fw::CmdResponse::OK);
        ASSERT_from_framWrite_SIZE(0);
    }
}

void FramPeekPokeTester::testPoke() {
    const U32 values[] = {0xA5, 0xBEEF, 0xDEADBEEF};
    for (FwSizeType w = 0; w < 3; w++) {
        const FramWordWidth width = WIDTHS[w];
        const FwSizeType size = static_cast<FwSizeType>(width.e);
        const U32 offset = static_cast<U32>(STest::Pick::lowerUpper(1, FRAM_SIZE - size - 1));
        const U8 before = this->m_fram[offset - 1];
        const U8 after = this->m_fram[offset + size];
        const U32 cmdSeq = STest::Pick::any();
        this->sendPoke(cmdSeq, offset, width, values[w]);
        ASSERT_CMD_RESPONSE_SIZE(0);
        ASSERT_from_framWrite_SIZE(1);
        this->complete();

        // Big endian, exactly `size` bytes
        for (FwSizeType i = 0; i < size; i++) {
            ASSERT_EQ(this->m_fram[offset + i], static_cast<U8>(values[w] >> (8 * (size - 1 - i))));
        }
        ASSERT_EQ(this->m_fram[offset - 1], before);
        ASSERT_EQ(this->m_fram[offset + size], after);
        ASSERT_EVENTS_Poked_SIZE(1);
        ASSERT_EVENTS_Poked(0, offset, width, values[w]);
        this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, cmdSeq, Fw::CmdResponse::OK);

        // Reads back the same value
        this->sendPeek(cmdSeq, offset, width);
        this->complete();
        ASSERT_EVENTS_Peeked(0, offset, width, values[w]);
    }
}

void FramPeekPokeTester::testPokeTooWide() {
    const U32 tooWide[] = {0x100, 0x10000};
    for (FwSizeType w = 0; w < 2; w++) {
        const U32 cmdSeq = STest::Pick::any();
        this->sendPoke(cmdSeq, 0, WIDTHS[w], tooWide[w]);
        ASSERT_EVENTS_ValueTooWide_SIZE(1);
        ASSERT_EVENTS_ValueTooWide(0, WIDTHS[w], tooWide[w]);
        this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        ASSERT_from_framWrite_SIZE(0);
    }
    // The largest value of each width fits
    this->sendPoke(0, 0, FramWordWidth::U8_WORD, 0xFF);
    this->complete();
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, 0, Fw::CmdResponse::OK);
    this->sendPoke(0, 0, FramWordWidth::U16_WORD, 0xFFFF);
    this->complete();
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, 0, Fw::CmdResponse::OK);
}

void FramPeekPokeTester::testAccessFailed() {
    this->sendPeek(1, 16, FramWordWidth::U32_WORD);
    this->complete(Fram::Status::SPI_ERROR);
    ASSERT_EVENTS_Peeked_SIZE(0);
    ASSERT_EVENTS_AccessFailed_SIZE(1);
    ASSERT_EVENTS_AccessFailed(0, Fram::Op::READ, 16, FramWordWidth::U32_WORD, Fram::Status::SPI_ERROR);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_PEEK, 1, Fw::CmdResponse::EXECUTION_ERROR);

    this->sendPoke(2, 16, FramWordWidth::U16_WORD, 0x1234);
    this->complete(Fram::Status::SPI_ERROR);
    ASSERT_EVENTS_Poked_SIZE(0);
    ASSERT_EVENTS_AccessFailed(0, Fram::Op::WRITE, 16, FramWordWidth::U16_WORD, Fram::Status::SPI_ERROR);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, 2, Fw::CmdResponse::EXECUTION_ERROR);
}

void FramPeekPokeTester::testRequestRejected() {
    const Fram::Status statuses[] = {Fram::Status::BUSY, Fram::Status::OUT_OF_RANGE, Fram::Status::WRITE_PROTECTED,
                                     Fram::Status::SPI_ERROR};
    for (const Fram::Status status : statuses) {
        this->m_reject = status;
        this->sendPoke(3, 32, FramWordWidth::U8_WORD, 0x7);
        ASSERT_FALSE(this->m_pending);
        ASSERT_EVENTS_AccessFailed_SIZE(1);
        ASSERT_EVENTS_AccessFailed(0, Fram::Op::WRITE, 32, FramWordWidth::U8_WORD, status);
        this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, 3, Fw::CmdResponse::EXECUTION_ERROR);
    }
    // Idle again afterwards
    this->sendPeek(4, 32, FramWordWidth::U8_WORD);
    this->complete();
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_PEEK, 4, Fw::CmdResponse::OK);
}

void FramPeekPokeTester::testBusy() {
    this->sendPeek(5, 8, FramWordWidth::U16_WORD);
    this->sendPoke(6, 8, FramWordWidth::U8_WORD, 1);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, 6, Fw::CmdResponse::BUSY);
    this->sendPeek(7, 8, FramWordWidth::U8_WORD);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_PEEK, 7, Fw::CmdResponse::BUSY);
    ASSERT_from_framWrite_SIZE(0);
    ASSERT_from_framRead_SIZE(0);

    // The outstanding PEEK reports its own word
    this->clearHistory();
    this->complete();
    ASSERT_EVENTS_Peeked(0, 8, FramWordWidth::U16_WORD, 0x0809);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_PEEK, 5, Fw::CmdResponse::OK);
}

void FramPeekPokeTester::testInCallCompletion() {
    this->m_completeInCall = true;
    this->sendPoke(8, 100, FramWordWidth::U32_WORD, 0x01020304);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_POKE, 8, Fw::CmdResponse::OK);
    this->sendPeek(9, 100, FramWordWidth::U32_WORD);
    ASSERT_EVENTS_Peeked(0, 100, FramWordWidth::U32_WORD, 0x01020304);
    this->assertResponse(FramPeekPokeComponentBase::OPCODE_PEEK, 9, Fw::CmdResponse::OK);
}

}  // namespace Samd21

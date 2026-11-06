// ======================================================================
// \title  PrmStorageTester.cpp
// \author tumbar
// \brief  cpp file for PrmStorage component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/PrmStorage/test/ut/PrmStorageTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"

#include <cstring>

namespace Samd21 {

// Definitions of the ODR-used class constants (C++14)
constexpr U32 PrmStorageTester::PRIME_OFFSET;
constexpr U32 PrmStorageTester::BACKUP_OFFSET;
constexpr FwSizeType PrmStorageTester::IMAGE_SIZE;
constexpr FwSizeType PrmStorageTester::FRAM_SIZE;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

PrmStorageTester::PrmStorageTester()
    : PrmStorageGTestBase("PrmStorageTester", PrmStorageTester::MAX_HISTORY_SIZE),
      component("PrmStorage"),
      m_fram(),
      m_image(),
      m_pending(),
      m_completeInCall(false),
      m_rejectStatus(Fram::Status::ACCEPTED),
      m_rejectCount(0),
      m_failRequestNumber(0),
      m_requests(0) {
    this->initComponents();
    this->connectPorts();
    this->m_pending.valid = false;
}

PrmStorageTester::~PrmStorageTester() {}

// ----------------------------------------------------------------------
// Simulated F-RAM
// ----------------------------------------------------------------------

Fram::Status PrmStorageTester::from_framRead_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->pushFromPortEntry_framRead(offset, buffer);
    return this->framRequest(Fram::Op::READ, offset, buffer);
}

Fram::Status PrmStorageTester::from_framWrite_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->pushFromPortEntry_framWrite(offset, buffer);
    return this->framRequest(Fram::Op::WRITE, offset, buffer);
}

Fram::Status PrmStorageTester::framRequest(Fram::Op op, U32 offset, Fw::Buffer& buffer) {
    // One request outstanding at a time, within the F-RAM driver's limits
    EXPECT_FALSE(this->m_pending.valid);
    EXPECT_GT(buffer.getSize(), 0U);
    EXPECT_LE(buffer.getSize(), static_cast<FwSizeType>(Fram::FramConfig::MaxTransfer));
    EXPECT_LE(offset + buffer.getSize(), FRAM_SIZE);

    if (this->m_rejectCount > 0) {
        this->m_rejectCount--;
        return this->m_rejectStatus;
    }

    this->m_requests++;
    if (op == Fram::Op::READ) {
        (void)::memcpy(buffer.getData(), &this->m_fram[offset], buffer.getSize());
    } else {
        (void)::memcpy(&this->m_fram[offset], buffer.getData(), buffer.getSize());
    }
    this->m_pending.valid = true;
    this->m_pending.op = op;
    this->m_pending.offset = offset;
    this->m_pending.buffer = buffer;
    if (this->m_completeInCall) {
        this->drain();
    }
    return Fram::Status::ACCEPTED;
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void PrmStorageTester::fillImage(U8 seed, FwSizeType size) {
    for (FwSizeType i = 0; i < size; i++) {
        this->m_image[i] = static_cast<U8>(seed + (i * 7));
    }
}

void PrmStorageTester::assertPattern(const U8* data, U8 seed, FwSizeType size) {
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(data[i], static_cast<U8>(seed + (i * 7))) << "byte " << i;
    }
}

void PrmStorageTester::sendSave(FwSizeType size) {
    this->clearHistory();
    Fw::Buffer image(this->m_image, size);
    this->invoke_to_writeIn(0, image);
}

void PrmStorageTester::sendLoad(FwSizeType size) {
    this->clearHistory();
    Fw::Buffer image(this->m_image, size);
    this->invoke_to_readIn(0, image);
}

void PrmStorageTester::sendSelectBank(PrmBank bank, U32 cmdSeq) {
    this->clearHistory();
    this->sendCmd_SELECT_BANK(TEST_INSTANCE_ID, cmdSeq, bank);
}

void PrmStorageTester::drain() {
    while (this->m_pending.valid) {
        const Fram::Status status =
            (this->m_requests == this->m_failRequestNumber) ? Fram::Status::SPI_ERROR : Fram::Status::OK;
        Pending pending = this->m_pending;
        this->m_pending.valid = false;
        if (pending.op == Fram::Op::READ) {
            this->invoke_to_framReadComplete(0, pending.offset, pending.buffer, status);
        } else {
            this->invoke_to_framWriteComplete(0, pending.offset, pending.buffer, status);
        }
    }
}

void PrmStorageTester::assertSaveDone(Drv::ByteStreamStatus status, FwSizeType size) {
    ASSERT_from_writeReturnOut_SIZE(1);
    ASSERT_from_writeReturnOut(0, Fw::Buffer(this->m_image, size), status);
    ASSERT_from_readReturnOut_SIZE(0);
}

void PrmStorageTester::assertLoadDone(Drv::ByteStreamStatus status, FwSizeType size) {
    ASSERT_from_readReturnOut_SIZE(1);
    ASSERT_from_readReturnOut(0, Fw::Buffer(this->m_image, size), status);
    ASSERT_from_writeReturnOut_SIZE(0);
}

void PrmStorageTester::assertChunks(U32 bankOffset, FwSizeType size) const {
    const FwSizeType chunk = Fram::FramConfig::MaxTransfer;
    const FwSizeType chunks = (size + chunk - 1) / chunk;
    const bool write = this->fromPortHistory_framWrite->size() > 0;
    ASSERT_EQ(write ? this->fromPortHistory_framWrite->size() : this->fromPortHistory_framRead->size(), chunks);
    for (FwSizeType c = 0; c < chunks; c++) {
        const FwSizeType expected = ((c + 1) * chunk <= size) ? chunk : (size - (c * chunk));
        const U32 offset =
            write ? this->fromPortHistory_framWrite->at(c).offset : this->fromPortHistory_framRead->at(c).offset;
        const Fw::Buffer& buffer =
            write ? this->fromPortHistory_framWrite->at(c).buffer : this->fromPortHistory_framRead->at(c).buffer;
        ASSERT_EQ(offset, static_cast<U32>(bankOffset + (c * chunk)));
        ASSERT_EQ(buffer.getData(), this->m_image + (c * chunk));
        ASSERT_EQ(buffer.getSize(), expected);
    }
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void PrmStorageTester::testSave() {
    this->fillImage(0x10);
    this->sendSave();
    // Asynchronous: nothing returns until the F-RAM completes
    ASSERT_from_framWrite_SIZE(1);
    ASSERT_from_writeReturnOut_SIZE(0);
    this->drain();
    this->assertSaveDone(Drv::ByteStreamStatus::OP_OK);
    this->assertChunks(PRIME_OFFSET);
    ASSERT_from_framRead_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    // The image is stored verbatim
    assertPattern(&this->m_fram[PRIME_OFFSET], 0x10);
}

void PrmStorageTester::testLoad() {
    for (FwSizeType i = 0; i < IMAGE_SIZE; i++) {
        this->m_fram[PRIME_OFFSET + i] = static_cast<U8>(0x20 + (i * 7));
    }
    this->sendLoad();
    this->drain();
    this->assertLoadDone(Drv::ByteStreamStatus::OP_OK);
    this->assertChunks(PRIME_OFFSET);
    ASSERT_from_framWrite_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
    assertPattern(this->m_image, 0x20);
}

void PrmStorageTester::testSelectBank() {
    this->fillImage(0x30);
    this->sendSave();
    this->drain();

    const U32 cmdSeq = STest::Pick::any();
    this->sendSelectBank(PrmBank::BACKUP, cmdSeq);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, PrmStorageComponentBase::OPCODE_SELECT_BANK, cmdSeq, Fw::CmdResponse::OK);
    // Selecting a bank is routine (every save cycle): telemetry only, no event
    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_ActiveBank_SIZE(1);
    ASSERT_TLM_ActiveBank(0, PrmBank::BACKUP);

    // The save goes to BACKUP, PRIME keeps the old image
    this->fillImage(0x31);
    this->sendSave();
    this->drain();
    this->assertSaveDone(Drv::ByteStreamStatus::OP_OK);
    this->assertChunks(BACKUP_OFFSET);
    assertPattern(&this->m_fram[BACKUP_OFFSET], 0x31);
    assertPattern(&this->m_fram[PRIME_OFFSET], 0x30);

    // Back to PRIME: the load returns its image
    this->sendSelectBank(PrmBank::PRIME, cmdSeq);
    ASSERT_TLM_ActiveBank(0, PrmBank::PRIME);
    this->sendLoad();
    this->drain();
    this->assertLoadDone(Drv::ByteStreamStatus::OP_OK);
    this->assertChunks(PRIME_OFFSET);
    assertPattern(this->m_image, 0x30);
}

void PrmStorageTester::testSelectBankBusy() {
    this->fillImage(0x40);
    this->sendSave();
    ASSERT_TRUE(this->m_pending.valid);

    const U32 cmdSeq = STest::Pick::any();
    this->sendSelectBank(PrmBank::BACKUP, cmdSeq);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, PrmStorageComponentBase::OPCODE_SELECT_BANK, cmdSeq, Fw::CmdResponse::BUSY);
    ASSERT_EVENTS_SIZE(0);
    ASSERT_TLM_SIZE(0);

    // The rest of the save stays on PRIME
    this->drain();
    ASSERT_from_writeReturnOut_SIZE(1);
    assertPattern(&this->m_fram[PRIME_OFFSET], 0x40);
    for (FwSizeType i = 0; i < IMAGE_SIZE; i++) {
        ASSERT_EQ(this->m_fram[BACKUP_OFFSET + i], 0);
    }
}

void PrmStorageTester::testStorageFailure() {
    const Fram::Op ops[] = {Fram::Op::WRITE, Fram::Op::READ};
    for (const Fram::Op op : ops) {
        // Fail the second chunk
        this->m_failRequestNumber = this->m_requests + 2;
        if (op == Fram::Op::WRITE) {
            this->sendSave();
        } else {
            this->sendLoad();
        }
        this->drain();
        this->m_failRequestNumber = 0;
        if (op == Fram::Op::WRITE) {
            this->assertSaveDone(Drv::ByteStreamStatus::OTHER_ERROR);
            ASSERT_from_framWrite_SIZE(2);
        } else {
            this->assertLoadDone(Drv::ByteStreamStatus::OTHER_ERROR);
            ASSERT_from_framRead_SIZE(2);
        }
        ASSERT_EVENTS_StorageFailed_SIZE(1);
        ASSERT_EVENTS_StorageFailed(0, op, PrmBank::PRIME, PRIME_OFFSET + Fram::FramConfig::MaxTransfer,
                                    Fram::Status::SPI_ERROR);
    }
}

void PrmStorageTester::testBusyRetry() {
    this->fillImage(0x50);
    const U32 rejections = static_cast<U32>(STest::Pick::lowerUpper(1, PrmStorageConfig::BUSY_RETRY_LIMIT));
    this->m_rejectStatus = Fram::Status::BUSY;
    this->m_rejectCount = rejections;
    this->sendSave();
    for (U32 i = 0; i < rejections; i++) {
        // Nothing accepted yet: the request waits for schedIn
        ASSERT_FALSE(this->m_pending.valid);
        ASSERT_from_writeReturnOut_SIZE(0);
        this->invoke_to_schedIn(0, 0);
    }
    ASSERT_TRUE(this->m_pending.valid);
    this->drain();
    this->assertSaveDone(Drv::ByteStreamStatus::OP_OK);
    ASSERT_EVENTS_SIZE(0);
    assertPattern(&this->m_fram[PRIME_OFFSET], 0x50);

    // A schedIn with nothing to retry does nothing
    this->clearHistory();
    this->invoke_to_schedIn(0, 0);
    ASSERT_FROM_PORT_HISTORY_SIZE(0);
    ASSERT_EVENTS_SIZE(0);
}

void PrmStorageTester::testBusyRetryExhausted() {
    this->m_rejectStatus = Fram::Status::BUSY;
    this->m_rejectCount = PrmStorageConfig::BUSY_RETRY_LIMIT + 1;
    this->sendLoad();
    for (U32 i = 0; i < PrmStorageConfig::BUSY_RETRY_LIMIT; i++) {
        ASSERT_from_readReturnOut_SIZE(0);
        this->invoke_to_schedIn(0, 0);
    }
    this->assertLoadDone(Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_EVENTS_StorageFailed_SIZE(1);
    ASSERT_EVENTS_StorageFailed(0, Fram::Op::READ, PrmBank::PRIME, PRIME_OFFSET, Fram::Status::BUSY);
    // The transfer is over: no more retries
    this->invoke_to_schedIn(0, 0);
    ASSERT_from_framRead_SIZE(PrmStorageConfig::BUSY_RETRY_LIMIT + 1);
}

void PrmStorageTester::testRequestRejected() {
    this->m_rejectStatus = Fram::Status::WRITE_PROTECTED;
    this->m_rejectCount = 1;
    this->sendSave();
    // Failed inside the call, no retry
    this->assertSaveDone(Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_EVENTS_StorageFailed_SIZE(1);
    ASSERT_EVENTS_StorageFailed(0, Fram::Op::WRITE, PrmBank::PRIME, PRIME_OFFSET, Fram::Status::WRITE_PROTECTED);
    this->invoke_to_schedIn(0, 0);
    ASSERT_from_framWrite_SIZE(1);
    ASSERT_from_writeReturnOut_SIZE(1);
}

void PrmStorageTester::testInCallCompletion() {
    this->m_completeInCall = true;
    this->fillImage(0x70);
    this->sendSave();
    this->assertSaveDone(Drv::ByteStreamStatus::OP_OK);
    (void)::memset(this->m_image, 0, sizeof(this->m_image));
    this->sendLoad();
    this->assertLoadDone(Drv::ByteStreamStatus::OP_OK);
    assertPattern(this->m_image, 0x70);
}

}  // namespace Samd21

// ======================================================================
// \title  StaticPrmDbTester.cpp
// \author tumbar
// \brief  cpp file for StaticPrmDb component test harness implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticPrmDb/test/ut/StaticPrmDbTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"

#include <cstring>

namespace Samd21 {

// ----------------------------------------------------------------------
// Test layout, standing in for the autocoded deployment (static-prm-db)
// ----------------------------------------------------------------------

// The stand-in splits B into an 8-byte and a 4-byte record
static_assert(PrmRecord::VALUE_SIZE == 8, "The test layout assumes the default StaticPrmDbConfig.RECORD_VALUE_SIZE");

// Hand-written equivalent of the generated code for three parameters: A (4 bytes, 1 record),
// B (12 bytes, 2 records) and C (1 byte, 1 record). Validity bits: A=0, B=1..2, C=3.

namespace {

U32 testValidity[1] = {};
struct {
    U8 a[StaticPrmDbTester::SIZE_A];
    U8 b[StaticPrmDbTester::SIZE_B];
    U8 c[StaticPrmDbTester::SIZE_C];
} testValues = {};

constexpr U32 MASK_A = 0x1;
constexpr U32 MASK_B = 0x6;
constexpr U32 MASK_C = 0x8;

bool validMask(U32 mask) {
    return (testValidity[0] & mask) == mask;
}

//! Value memory and validity mask of a test parameter
U8* testParam(FwPrmIdType id, FwSizeType& size, U32& mask) {
    switch (id) {
        case StaticPrmDbTester::PRM_A:
            size = StaticPrmDbTester::SIZE_A;
            mask = MASK_A;
            return testValues.a;
        case StaticPrmDbTester::PRM_B:
            size = StaticPrmDbTester::SIZE_B;
            mask = MASK_B;
            return testValues.b;
        case StaticPrmDbTester::PRM_C:
            size = StaticPrmDbTester::SIZE_C;
            mask = MASK_C;
            return testValues.c;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(id));
            return nullptr;
    }
}

StaticPrmDb::RecordLoad loadSegment(const U8* record, bool merge, U8* dest, FwSizeType length, U32 bit) {
    if (merge && ((testValidity[0] & bit) != 0)) {
        return StaticPrmDb::RecordLoad::KEPT;
    }
    (void)::memcpy(dest, &record[PrmRecord::VALUE_OFFSET], length);
    testValidity[0] |= bit;
    return StaticPrmDb::RecordLoad::LOADED;
}

}  // namespace

Fw::ParamValid StaticPrmDb::readParam(FwPrmIdType id, Fw::ParamBuffer& val) {
    FwSizeType size = 0;
    U32 mask = 0;
    U8* value = testParam(id, size, mask);
    if (!validMask(mask)) {
        return Fw::ParamValid::INVALID;
    }
    FW_ASSERT(val.setBuff(value, size) == Fw::FW_SERIALIZE_OK);
    return Fw::ParamValid::VALID;
}

void StaticPrmDb::writeParam(FwPrmIdType id, const Fw::ParamBuffer& val) {
    FwSizeType size = 0;
    U32 mask = 0;
    U8* value = testParam(id, size, mask);
    FW_ASSERT(val.getSize() == size, static_cast<FwAssertArgType>(val.getSize()));
    (void)::memcpy(value, val.getBuffAddr(), size);
    testValidity[0] |= mask;
}

StaticPrmDb::RecordLoad StaticPrmDb::loadRecord(const U8* record, bool merge) {
    switch (PrmRecord::getId(record)) {
        case StaticPrmDbTester::RECORD_A:
            return loadSegment(record, merge, testValues.a, 4, 0x1);
        case StaticPrmDbTester::RECORD_B0:
            return loadSegment(record, merge, &testValues.b[0], 8, 0x2);
        case StaticPrmDbTester::RECORD_B1:
            return loadSegment(record, merge, &testValues.b[8], 4, 0x4);
        case StaticPrmDbTester::RECORD_C:
            return loadSegment(record, merge, testValues.c, 1, 0x8);
        default:
            return RecordLoad::UNKNOWN;
    }
}

bool StaticPrmDb::saveRecord(FwSizeType index, U8* record) {
    switch (index) {
        case 0:
            if (!validMask(MASK_A)) {
                return false;
            }
            PrmRecord::pack(record, StaticPrmDbTester::RECORD_A, testValues.a, 4);
            return true;
        case 1:
            if (!validMask(MASK_B)) {
                return false;
            }
            PrmRecord::pack(record, StaticPrmDbTester::RECORD_B0, &testValues.b[0], 8);
            return true;
        case 2:
            if (!validMask(MASK_B)) {
                return false;
            }
            PrmRecord::pack(record, StaticPrmDbTester::RECORD_B1, &testValues.b[8], 4);
            return true;
        case 3:
            if (!validMask(MASK_C)) {
                return false;
            }
            PrmRecord::pack(record, StaticPrmDbTester::RECORD_C, testValues.c, 1);
            return true;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(index));
            return false;
    }
}

void StaticPrmDb::invalidateAll() {
    testValidity[0] = 0;
}

U32 StaticPrmDb::countInvalidParams() {
    return (validMask(MASK_A) ? 0U : 1U) + (validMask(MASK_B) ? 0U : 1U) + (validMask(MASK_C) ? 0U : 1U);
}

U32 StaticPrmDb::numParams() {
    return 3;
}

FwSizeType StaticPrmDb::numRecords() {
    return 4;
}

void StaticPrmDb::loadParameters() {
    StaticPrmDbTester::s_loadParametersCalls++;
}

// Definitions of the ODR-used class constants (C++14)
constexpr FwPrmIdType StaticPrmDbTester::PRM_A;
constexpr FwPrmIdType StaticPrmDbTester::PRM_B;
constexpr FwPrmIdType StaticPrmDbTester::PRM_C;
constexpr FwSizeType StaticPrmDbTester::SIZE_A;
constexpr FwSizeType StaticPrmDbTester::SIZE_B;
constexpr FwSizeType StaticPrmDbTester::SIZE_C;
constexpr U16 StaticPrmDbTester::RECORD_A;
constexpr U16 StaticPrmDbTester::RECORD_B0;
constexpr U16 StaticPrmDbTester::RECORD_B1;
constexpr U16 StaticPrmDbTester::RECORD_C;
constexpr FwSizeType StaticPrmDbTester::STORE_RECORDS;
constexpr FwSizeType StaticPrmDbTester::STORE_SIZE;

U32 StaticPrmDbTester::s_loadParametersCalls = 0;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

StaticPrmDbTester::StaticPrmDbTester()
    : StaticPrmDbGTestBase("StaticPrmDbTester", StaticPrmDbTester::MAX_HISTORY_SIZE),
      component("StaticPrmDb"),
      m_store(),
      m_pending(),
      m_completeInCall(false),
      m_failTransferNumber(0),
      m_transfers(0) {
    this->initComponents();
    this->connectPorts();
    this->m_pending.valid = false;
    // The layout memory is global: start every test from a fresh database
    (void)::memset(testValidity, 0, sizeof(testValidity));
    (void)::memset(&testValues, 0, sizeof(testValues));
    s_loadParametersCalls = 0;
}

StaticPrmDbTester::~StaticPrmDbTester() {}

// ----------------------------------------------------------------------
// Simulated store
// ----------------------------------------------------------------------

void StaticPrmDbTester::from_writeOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->pushFromPortEntry_writeOut(offset, buffer);
    EXPECT_FALSE(this->m_pending.valid);
    EXPECT_LE(offset + buffer.getSize(), STORE_SIZE);
    this->m_transfers++;
    (void)::memcpy(&this->m_store[offset], buffer.getData(), buffer.getSize());
    this->m_pending.valid = true;
    this->m_pending.write = true;
    this->m_pending.buffer = buffer;
    if (this->m_completeInCall) {
        this->drain();
    }
}

void StaticPrmDbTester::from_readOut_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->pushFromPortEntry_readOut(offset, buffer);
    EXPECT_FALSE(this->m_pending.valid);
    EXPECT_LE(offset + buffer.getSize(), STORE_SIZE);
    this->m_transfers++;
    (void)::memcpy(buffer.getData(), &this->m_store[offset], buffer.getSize());
    this->m_pending.valid = true;
    this->m_pending.write = false;
    this->m_pending.buffer = buffer;
    if (this->m_completeInCall) {
        this->drain();
    }
}

void StaticPrmDbTester::drain() {
    while (this->m_pending.valid) {
        const Drv::ByteStreamStatus status = (this->m_transfers == this->m_failTransferNumber)
                                                 ? Drv::ByteStreamStatus::OTHER_ERROR
                                                 : Drv::ByteStreamStatus::OP_OK;
        Pending pending = this->m_pending;
        this->m_pending.valid = false;
        if (pending.write) {
            this->invoke_to_writeReturnIn(0, pending.buffer, status);
        } else {
            this->invoke_to_readReturnIn(0, pending.buffer, status);
        }
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void StaticPrmDbTester::setParam(FwPrmIdType id, FwSizeType size, U8 seed) {
    if (size > FW_PARAM_BUFFER_MAX_SIZE) {
        // Too large for an Fw::ParamBuffer in this configuration: stand in for setPrm through
        // the layout memory so the record split is still covered
        FwSizeType paramSize = 0;
        U32 mask = 0;
        U8* value = testParam(id, paramSize, mask);
        ASSERT_EQ(paramSize, size);
        for (FwSizeType i = 0; i < size; i++) {
            value[i] = static_cast<U8>(seed + (i * 13));
        }
        testValidity[0] |= mask;
        return;
    }
    Fw::ParamBuffer val;
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(val.serializeFrom(static_cast<U8>(seed + (i * 13))), Fw::FW_SERIALIZE_OK);
    }
    this->invoke_to_setPrm(0, id, val);
}

void StaticPrmDbTester::assertParam(FwPrmIdType id, FwSizeType size, Fw::ParamValid valid, U8 seed) {
    if (size > FW_PARAM_BUFFER_MAX_SIZE) {
        // Too large for an Fw::ParamBuffer in this configuration: read the layout memory
        FwSizeType paramSize = 0;
        U32 mask = 0;
        const U8* value = testParam(id, paramSize, mask);
        const bool allValid = validMask(mask);
        ASSERT_EQ(allValid, valid == Fw::ParamValid::VALID) << "parameter 0x" << std::hex << id;
        if (allValid) {
            for (FwSizeType i = 0; i < size; i++) {
                ASSERT_EQ(value[i], static_cast<U8>(seed + (i * 13))) << "byte " << i;
            }
        }
        return;
    }
    Fw::ParamBuffer val;
    ASSERT_EQ(this->invoke_to_getPrm(0, id, val), valid) << "parameter 0x" << std::hex << id;
    if (valid == Fw::ParamValid::VALID) {
        ASSERT_EQ(val.getSize(), size);
        for (FwSizeType i = 0; i < size; i++) {
            ASSERT_EQ(val.getBuffAddr()[i], static_cast<U8>(seed + (i * 13))) << "byte " << i;
        }
    }
}

void StaticPrmDbTester::save(Fw::CmdResponse response) {
    this->clearHistory();
    const U32 cmdSeq = STest::Pick::any();
    this->sendCmd_SAVE(TEST_INSTANCE_ID, cmdSeq);
    this->drain();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, StaticPrmDbComponentBase::OPCODE_SAVE, cmdSeq, response);
}

void StaticPrmDbTester::load(StaticPrmDb_LoadMode mode, Fw::CmdResponse response) {
    this->clearHistory();
    const U32 cmdSeq = STest::Pick::any();
    this->sendCmd_LOAD(TEST_INSTANCE_ID, cmdSeq, mode);
    this->drain();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, StaticPrmDbComponentBase::OPCODE_LOAD, cmdSeq, response);
}

FwSizeType StaticPrmDbTester::findSlot(U16 recordId) const {
    for (FwSizeType s = 0; s < STORE_RECORDS; s++) {
        if (PrmRecord::getId(&this->m_store[s * PrmRecord::SIZE]) == recordId) {
            return s;
        }
    }
    return STORE_RECORDS;
}

void StaticPrmDbTester::assertStoreRecords(const U16* ids, FwSizeType count) const {
    FwSizeType found = 0;
    for (FwSizeType s = 0; s < STORE_RECORDS; s++) {
        const U8* record = &this->m_store[s * PrmRecord::SIZE];
        if (PrmRecord::isEmpty(record)) {
            continue;
        }
        ASSERT_TRUE(PrmRecord::crcValid(record)) << "slot " << s;
        bool expected = false;
        for (FwSizeType i = 0; i < count; i++) {
            expected = expected || (PrmRecord::getId(record) == ids[i]);
        }
        ASSERT_TRUE(expected) << "slot " << s << " id 0x" << std::hex << PrmRecord::getId(record);
        found++;
    }
    ASSERT_EQ(found, count);
}

void StaticPrmDbTester::assertTransfers(bool write) const {
    const FwSizeType transfers =
        (STORE_RECORDS + StaticPrmDbConfig::SCRATCH_RECORDS - 1) / StaticPrmDbConfig::SCRATCH_RECORDS;
    ASSERT_EQ(write ? this->fromPortHistory_writeOut->size() : this->fromPortHistory_readOut->size(), transfers);
    for (FwSizeType t = 0; t < transfers; t++) {
        const U32 offset =
            write ? this->fromPortHistory_writeOut->at(t).offset : this->fromPortHistory_readOut->at(t).offset;
        const Fw::Buffer& buffer =
            write ? this->fromPortHistory_writeOut->at(t).buffer : this->fromPortHistory_readOut->at(t).buffer;
        const FwSizeType left = STORE_RECORDS - (t * StaticPrmDbConfig::SCRATCH_RECORDS);
        const FwSizeType records =
            (left < StaticPrmDbConfig::SCRATCH_RECORDS) ? left : StaticPrmDbConfig::SCRATCH_RECORDS;
        ASSERT_EQ(offset, t * StaticPrmDbConfig::SCRATCH_RECORDS * PrmRecord::SIZE);
        ASSERT_EQ(buffer.getSize(), records * PrmRecord::SIZE);
    }
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void StaticPrmDbTester::testSetGet() {
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::INVALID);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::INVALID);
}

void StaticPrmDbTester::testSaveLoad() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();
    this->assertTransfers(true);
    ASSERT_EVENTS_SavedDatabase_SIZE(1);
    ASSERT_EVENTS_SavedDatabase(0, 3, 0);
    const U16 ids[] = {RECORD_A, RECORD_B0, RECORD_B1, RECORD_C};
    this->assertStoreRecords(ids, 4);

    // Lose everything in memory, then restore it
    this->setParam(PRM_A, SIZE_A, 0x40);
    this->load(StaticPrmDb_LoadMode::REPLACE);
    this->assertTransfers(false);
    ASSERT_EQ(s_loadParametersCalls, 1U);
    ASSERT_EVENTS_LoadedDatabase_SIZE(1);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, 4, 0, 0, 0);
    ASSERT_TLM_ValidRecords(0, 4);
    ASSERT_TLM_CorruptRecords(0, 0);
    ASSERT_TLM_InvalidParameters(0, 0);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x30);
}

void StaticPrmDbTester::testBitFlip() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();

    // Flip one bit anywhere in the second record of PRM_B (ID, CRC or value)
    const FwSizeType slot = this->findSlot(RECORD_B1);
    ASSERT_LT(slot, STORE_RECORDS);
    const FwSizeType bit = STest::Pick::lowerUpper(0, (PrmRecord::SIZE * 8) - 1);
    this->m_store[(slot * PrmRecord::SIZE) + (bit / 8)] ^= static_cast<U8>(1 << (bit % 8));

    this->load(StaticPrmDb_LoadMode::REPLACE);
    // A flip in the ID may land on an empty or unknown ID rather than fail the CRC; either way
    // only PRM_B loses its value
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x30);
    ASSERT_TLM_ValidRecords(0, 3);
    ASSERT_TLM_InvalidParameters(0, 1);
}

void StaticPrmDbTester::testDictionaryChange() {
    // A store written by another dictionary: PRM_C's record first, a record of a removed
    // parameter, then PRM_A. PRM_B was added since and has no records.
    (void)::memset(this->m_store, 0, sizeof(this->m_store));
    const U8 valueC[] = {0x30};
    const U8 valueA[] = {0x10, 0x1D, 0x2A, 0x37};
    const U8 removed[] = {1, 2, 3, 4, 5, 6, 7, 8};
    PrmRecord::pack(&this->m_store[0 * PrmRecord::SIZE], RECORD_C, valueC, sizeof(valueC));
    PrmRecord::pack(&this->m_store[1 * PrmRecord::SIZE], 0x7777, removed, sizeof(removed));
    PrmRecord::pack(&this->m_store[4 * PrmRecord::SIZE], RECORD_A, valueA, sizeof(valueA));

    this->load(StaticPrmDb_LoadMode::REPLACE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, 2, 0, 1, 1);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x30);
}

void StaticPrmDbTester::testMerge() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();
    U8 backup[STORE_SIZE];
    (void)::memcpy(backup, this->m_store, sizeof(backup));

    // PRIME: newer A and C, with B's first record corrupt
    this->setParam(PRM_A, SIZE_A, 0x11);
    this->setParam(PRM_C, SIZE_C, 0x31);
    this->save();
    const FwSizeType slotB0 = this->findSlot(RECORD_B0);
    ASSERT_LT(slotB0, STORE_RECORDS);
    this->m_store[(slotB0 * PrmRecord::SIZE) + PrmRecord::VALUE_OFFSET] ^= 0x01;

    this->load(StaticPrmDb_LoadMode::REPLACE);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);
    ASSERT_TLM_CorruptRecords(0, 1);

    // BACKUP fills in B only: PRIME's A and C take precedence over BACKUP's older values
    (void)::memcpy(this->m_store, backup, sizeof(backup));
    this->load(StaticPrmDb_LoadMode::MERGE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::MERGE, 4, 0, 0, 0);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x11);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x31);
    ASSERT_EQ(s_loadParametersCalls, 2U);
}

void StaticPrmDbTester::testSaveSkipsInvalid() {
    // Fill every slot with a stale record of an earlier save
    const U8 stale[] = {0xEE};
    for (FwSizeType s = 0; s < STORE_RECORDS; s++) {
        PrmRecord::pack(&this->m_store[s * PrmRecord::SIZE], 0x6600 + static_cast<U16>(s), stale, 1);
    }

    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();
    ASSERT_EVENTS_SavedDatabase(0, 1, 2);
    const U16 ids[] = {RECORD_C};
    this->assertStoreRecords(ids, 1);
}

void StaticPrmDbTester::testBlankStore() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    (void)::memset(this->m_store, 0xFF, sizeof(this->m_store));
    this->load(StaticPrmDb_LoadMode::REPLACE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, 0, 0, 0, 3);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::INVALID);
}

void StaticPrmDbTester::testReadFailure() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();

    // Records are saved in parameter order; move C's record into the second transfer so a
    // failure there keeps A and B (loaded by the first transfer) and loses C
    const FwSizeType slotC = this->findSlot(RECORD_C);
    ASSERT_LT(slotC, StaticPrmDbConfig::SCRATCH_RECORDS);
    const FwSizeType moved = StaticPrmDbConfig::SCRATCH_RECORDS;
    (void)::memcpy(&this->m_store[moved * PrmRecord::SIZE], &this->m_store[slotC * PrmRecord::SIZE], PrmRecord::SIZE);
    PrmRecord::packEmpty(&this->m_store[slotC * PrmRecord::SIZE]);

    this->m_failTransferNumber = this->m_transfers + 2;
    this->load(StaticPrmDb_LoadMode::REPLACE, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_ReadFailed_SIZE(1);
    ASSERT_EVENTS_ReadFailed(0, Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_from_readOut_SIZE(2);
    ASSERT_EQ(s_loadParametersCalls, 1U);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::INVALID);
}

void StaticPrmDbTester::testWriteFailure() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->m_failTransferNumber = this->m_transfers + 1;
    this->save(Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_from_writeOut_SIZE(1);
    ASSERT_EVENTS_SavedDatabase_SIZE(0);
    // The values in memory are untouched
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
}

void StaticPrmDbTester::testBusy() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->clearHistory();
    this->sendCmd_SAVE(TEST_INSTANCE_ID, 1);
    ASSERT_TRUE(this->m_pending.valid);

    this->sendCmd_SAVE(TEST_INSTANCE_ID, 2);
    this->sendCmd_LOAD(TEST_INSTANCE_ID, 3, StaticPrmDb_LoadMode::MERGE);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, StaticPrmDbComponentBase::OPCODE_SAVE, 2, Fw::CmdResponse::BUSY);
    ASSERT_CMD_RESPONSE(1, StaticPrmDbComponentBase::OPCODE_LOAD, 3, Fw::CmdResponse::BUSY);

    this->setParam(PRM_A, SIZE_A, 0x40);
    ASSERT_EVENTS_DroppingPrmSave_SIZE(1);
    ASSERT_EVENTS_DroppingPrmSave(0, PRM_A, StaticPrmDb_IoState::WRITING);
    Fw::ParamBuffer val;
    ASSERT_EQ(this->invoke_to_getPrm(0, PRM_A, val), Fw::ParamValid::INVALID);

    this->drain();
    ASSERT_CMD_RESPONSE_SIZE(3);
    ASSERT_CMD_RESPONSE(2, StaticPrmDbComponentBase::OPCODE_SAVE, 1, Fw::CmdResponse::OK);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
}

void StaticPrmDbTester::testInCallCompletion() {
    this->m_completeInCall = true;
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->save();
    this->assertTransfers(true);
    this->load(StaticPrmDb_LoadMode::REPLACE);
    this->assertTransfers(false);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::INVALID);
}

}  // namespace Samd21

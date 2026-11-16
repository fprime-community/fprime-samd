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
// B (12 bytes and a 2-byte value check, 2 records: B:1 holds value bytes 8..11 then the check)
// and C (1 byte, 1 record). Validity bits: A=0, B=1..2, C=3. Like the generated code, it only
// maps IDs and indices to layouts; the work is done by the component's shared helpers.

static_assert(PrmRecord::storedSize(StaticPrmDbTester::SIZE_B) == 14, "B carries a value check");
static_assert(PrmRecord::recordCount(StaticPrmDbTester::SIZE_B) == 2, "B takes two records");

namespace {

U32 s_testValidity[1] = {};
U32 s_testLoaded[1] = {};
struct {
    U8 a[StaticPrmDbTester::SIZE_A];
    U8 b[StaticPrmDbTester::SIZE_B];
    U8 c[StaticPrmDbTester::SIZE_C];
} s_testValues = {};
struct {
    U16 b;
} s_testChecks = {};

constexpr StaticPrmDb::ParamLayout LAYOUT_A = {
    s_testValues.a, StaticPrmDbTester::SIZE_A, nullptr, &s_testValidity[0], &s_testLoaded[0], 0x1, 0};
constexpr StaticPrmDb::ParamLayout LAYOUT_B = {
    s_testValues.b, StaticPrmDbTester::SIZE_B, &s_testChecks.b, &s_testValidity[0], &s_testLoaded[0], 0x6, 1};
constexpr StaticPrmDb::ParamLayout LAYOUT_C = {
    s_testValues.c, StaticPrmDbTester::SIZE_C, nullptr, &s_testValidity[0], &s_testLoaded[0], 0x8, 3};

//! Layout of a test parameter
const StaticPrmDb::ParamLayout& testParam(FwPrmIdType id) {
    switch (id) {
        case StaticPrmDbTester::PRM_A:
            return LAYOUT_A;
        case StaticPrmDbTester::PRM_B:
            return LAYOUT_B;
        case StaticPrmDbTester::PRM_C:
            return LAYOUT_C;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(id));
            return LAYOUT_A;
    }
}

bool validMask(U32 mask) {
    return (s_testValidity[0] & mask) == mask;
}

}  // namespace

Fw::ParamValid StaticPrmDb::readParam(FwPrmIdType id, Fw::ParamBuffer& val) {
    return StaticPrmDb::readValue(testParam(id), val);
}

void StaticPrmDb::writeParam(FwPrmIdType id, const Fw::ParamBuffer& val) {
    StaticPrmDb::writeValue(testParam(id), val);
}

StaticPrmDb::RecordLoad StaticPrmDb::loadRecord(const U8* record, bool merge) {
    switch (PrmRecord::getId(record)) {
        case StaticPrmDbTester::RECORD_A:
            return StaticPrmDb::loadSegment(record, merge, LAYOUT_A, 0);
        case StaticPrmDbTester::RECORD_B0:
            return StaticPrmDb::loadSegment(record, merge, LAYOUT_B, 0);
        case StaticPrmDbTester::RECORD_B1:
            return StaticPrmDb::loadSegment(record, merge, LAYOUT_B, 1);
        case StaticPrmDbTester::RECORD_C:
            return StaticPrmDb::loadSegment(record, merge, LAYOUT_C, 0);
        default:
            return RecordLoad::UNKNOWN;
    }
}

bool StaticPrmDb::saveRecord(FwSizeType index, U8* record) {
    switch (index) {
        case 0:
            return StaticPrmDb::saveSegment(record, StaticPrmDbTester::RECORD_A, LAYOUT_A, 0);
        case 1:
            return StaticPrmDb::saveSegment(record, StaticPrmDbTester::RECORD_B0, LAYOUT_B, 0);
        case 2:
            return StaticPrmDb::saveSegment(record, StaticPrmDbTester::RECORD_B1, LAYOUT_B, 1);
        case 3:
            return StaticPrmDb::saveSegment(record, StaticPrmDbTester::RECORD_C, LAYOUT_C, 0);
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(index));
            return false;
    }
}

void StaticPrmDb::beginLoad() {
    s_testLoaded[0] = 0;
}

U32 StaticPrmDb::commitLoad(bool complete, bool merge) {
    U32 incoherent = 0;
    incoherent += StaticPrmDb::commitValue(complete, merge, LAYOUT_A);
    incoherent += StaticPrmDb::commitValue(complete, merge, LAYOUT_B);
    incoherent += StaticPrmDb::commitValue(complete, merge, LAYOUT_C);
    return incoherent;
}

U32 StaticPrmDb::countInvalidParams() {
    return (validMask(LAYOUT_A.mask) ? 0U : 1U) + (validMask(LAYOUT_B.mask) ? 0U : 1U) +
           (validMask(LAYOUT_C.mask) ? 0U : 1U);
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
constexpr U32 StaticPrmDbTester::CHAINED_CMD_SEQ;

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
      m_transfers(0),
      m_chainSave(false) {
    this->initComponents();
    this->connectPorts();
    this->m_pending.valid = false;
    // The layout memory is global: start every test from a fresh database
    (void)::memset(s_testValidity, 0, sizeof(s_testValidity));
    (void)::memset(s_testLoaded, 0, sizeof(s_testLoaded));
    (void)::memset(&s_testValues, 0, sizeof(s_testValues));
    (void)::memset(&s_testChecks, 0, sizeof(s_testChecks));
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
        this->complete();
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
        this->complete();
    }
}

void StaticPrmDbTester::complete() {
    ASSERT_TRUE(this->m_pending.valid);
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

void StaticPrmDbTester::run() {
    // Each pass completes the outstanding transfer or lets activeIn issue the next one; a store
    // takes at most this many transfers, two passes each, for each of two chained commands
    constexpr FwSizeType TRANSFERS =
        (STORE_RECORDS + StaticPrmDbConfig::SCRATCH_RECORDS - 1) / StaticPrmDbConfig::SCRATCH_RECORDS;
    constexpr FwSizeType MAX_PASSES = 4 * (TRANSFERS + 2);
    for (FwSizeType pass = 0; pass < MAX_PASSES; pass++) {
        const bool completed = this->m_pending.valid;
        if (completed) {
            this->complete();
        }
        const bool worked = this->invoke_to_activeIn(0, 0);
        if (!completed && !worked) {
            // Idle, or waiting on storage that has nothing pending: nothing left to run
            return;
        }
    }
    FAIL() << "the transfers did not finish in " << MAX_PASSES << " passes";
}

void StaticPrmDbTester::cmdResponseIn(FwOpcodeType opCode, U32 cmdSeq, Fw::CmdResponse response) {
    StaticPrmDbGTestBase::cmdResponseIn(opCode, cmdSeq, response);
    if (this->m_chainSave) {
        // A client that sends its next command from the response of the last one
        this->m_chainSave = false;
        this->sendCmd_SAVE(TEST_INSTANCE_ID, CHAINED_CMD_SEQ);
    }
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void StaticPrmDbTester::setParam(FwPrmIdType id, FwSizeType size, U8 seed) {
    Fw::ParamBuffer val;
    for (FwSizeType i = 0; i < size; i++) {
        ASSERT_EQ(val.serializeFrom(static_cast<U8>(seed + (i * 13))), Fw::FW_SERIALIZE_OK);
    }
    this->invoke_to_setPrm(0, id, val);
}

void StaticPrmDbTester::assertParam(FwPrmIdType id, FwSizeType size, Fw::ParamValid valid, U8 seed) {
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
    this->run();
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, StaticPrmDbComponentBase::OPCODE_SAVE, cmdSeq, response);
}

void StaticPrmDbTester::load(StaticPrmDb_LoadMode mode, Fw::CmdResponse response) {
    this->clearHistory();
    const U32 cmdSeq = STest::Pick::any();
    this->sendCmd_LOAD(TEST_INSTANCE_ID, cmdSeq, mode);
    this->run();
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

void StaticPrmDbTester::interruptSave(const U8* earlier, FwSizeType slots) {
    ASSERT_LE(slots, STORE_RECORDS);
    (void)::memcpy(&this->m_store[slots * PrmRecord::SIZE], &earlier[slots * PrmRecord::SIZE],
                   (STORE_RECORDS - slots) * PrmRecord::SIZE);
}

U8 StaticPrmDbTester::paramSeed(FwPrmIdType id, FwSizeType size, U8 seedA, U8 seedB) {
    Fw::ParamBuffer val;
    if (this->invoke_to_getPrm(0, id, val) == Fw::ParamValid::INVALID) {
        return 0;
    }
    EXPECT_EQ(val.getSize(), size);
    // Every byte must follow the same seed: a value mixed of two saves matches neither
    for (const U8 seed : {seedA, seedB}) {
        bool match = true;
        for (FwSizeType i = 0; i < size; i++) {
            match = match && (val.getBuffAddr()[i] == static_cast<U8>(seed + (i * 13)));
        }
        if (match) {
            return seed;
        }
    }
    ADD_FAILURE() << "parameter 0x" << std::hex << id << " is a mix of two saves";
    return 0;
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
    const U16 ids[] = {RECORD_A, RECORD_B0, RECORD_B1, RECORD_C};
    this->assertStoreRecords(ids, 4);

    // Lose everything in memory, then restore it
    this->setParam(PRM_A, SIZE_A, 0x40);
    this->load(StaticPrmDb_LoadMode::REPLACE);
    this->assertTransfers(false);
    ASSERT_EQ(s_loadParametersCalls, 1U);
    ASSERT_EVENTS_LoadedDatabase_SIZE(1);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::COMPLETE, 4, 0, 0, 0, 0);
    ASSERT_TLM_IncoherentParameters(0, 0);
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
    // A missing segment shows as a corrupt or unknown record, not as a mix of saves
    ASSERT_TLM_IncoherentParameters(0, 0);
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
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::COMPLETE, 2, 0, 1, 0, 1);
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

    // PRIME's intact B:1 was dropped with B, so BACKUP fills in B whole; PRIME's A and C take
    // precedence over BACKUP's older values
    (void)::memcpy(this->m_store, backup, sizeof(backup));
    this->load(StaticPrmDb_LoadMode::MERGE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::MERGE, StaticPrmDb_LoadResult::COMPLETE, 4, 0, 0, 0, 0);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x11);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x31);
    ASSERT_EQ(s_loadParametersCalls, 2U);
}

void StaticPrmDbTester::testInterruptedSave() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();
    U8 earlier[STORE_SIZE];
    (void)::memcpy(earlier, this->m_store, sizeof(earlier));
    this->setParam(PRM_A, SIZE_A, 0x40);
    this->setParam(PRM_B, SIZE_B, 0x50);
    this->setParam(PRM_C, SIZE_C, 0x60);
    this->save();
    U8 later[STORE_SIZE];
    (void)::memcpy(later, this->m_store, sizeof(later));

    // Both saves put B's records in the same two slots, so stopping between them leaves the
    // later B:0 with the earlier B:1: two records with good CRCs, of different values
    const FwSizeType slotB0 = this->findSlot(RECORD_B0);
    ASSERT_EQ(this->findSlot(RECORD_B1), slotB0 + 1);

    // Power lost after each record of the later save
    for (FwSizeType slots = 0; slots <= STORE_RECORDS; slots++) {
        (void)::memcpy(this->m_store, later, sizeof(later));
        this->interruptSave(earlier, slots);
        this->load(StaticPrmDb_LoadMode::REPLACE);

        const bool mixedB = (slots == slotB0 + 1);
        ASSERT_EVENTS_LoadedDatabase_SIZE(1);
        ASSERT_EQ(this->eventHistory_LoadedDatabase->at(0).incoherentParameters, mixedB ? 1U : 0U)
            << "interrupted after " << slots << " slots";
        ASSERT_TLM_IncoherentParameters(0, mixedB ? 1U : 0U);
        ASSERT_EQ(this->paramSeed(PRM_A, SIZE_A, 0x10, 0x40), (slots > this->findSlot(RECORD_A)) ? 0x40 : 0x10);
        ASSERT_EQ(this->paramSeed(PRM_B, SIZE_B, 0x20, 0x50), mixedB ? 0 : ((slots > slotB0) ? 0x50 : 0x20));
        ASSERT_EQ(this->paramSeed(PRM_C, SIZE_C, 0x30, 0x60), (slots > this->findSlot(RECORD_C)) ? 0x60 : 0x30);
    }
}

void StaticPrmDbTester::testMergeMixedSaves() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->save();
    U8 backup[STORE_SIZE];
    (void)::memcpy(backup, this->m_store, sizeof(backup));

    // PRIME: a newer B whose second record is corrupt. Its first record is good, but must not be
    // completed with BACKUP's second record of the older B.
    this->setParam(PRM_B, SIZE_B, 0x50);
    this->save();
    const FwSizeType slotB1 = this->findSlot(RECORD_B1);
    ASSERT_LT(slotB1, STORE_RECORDS);
    this->m_store[(slotB1 * PrmRecord::SIZE) + PrmRecord::VALUE_OFFSET] ^= 0x01;

    this->load(StaticPrmDb_LoadMode::REPLACE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::COMPLETE, 2, 1, 0, 0, 2);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);

    (void)::memcpy(this->m_store, backup, sizeof(backup));
    this->load(StaticPrmDb_LoadMode::MERGE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::MERGE, StaticPrmDb_LoadResult::COMPLETE, 3, 0, 0, 0, 1);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    ASSERT_EQ(this->paramSeed(PRM_B, SIZE_B, 0x20, 0x50), 0x20);

    // PRIME: an interrupted save mixing two B values. B is dropped whole, and BACKUP fills it in.
    this->setParam(PRM_B, SIZE_B, 0x70);
    this->save();
    U8 earlier[STORE_SIZE];
    (void)::memcpy(earlier, this->m_store, sizeof(earlier));
    this->setParam(PRM_B, SIZE_B, 0x50);
    this->save();
    this->interruptSave(earlier, this->findSlot(RECORD_B1));
    this->load(StaticPrmDb_LoadMode::REPLACE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::COMPLETE, 3, 0, 0, 1, 2);

    (void)::memcpy(this->m_store, backup, sizeof(backup));
    this->load(StaticPrmDb_LoadMode::MERGE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::MERGE, StaticPrmDb_LoadResult::COMPLETE, 3, 0, 0, 0, 1);
    ASSERT_EQ(this->paramSeed(PRM_B, SIZE_B, 0x20, 0x50), 0x20);
}

void StaticPrmDbTester::testStaleDuplicate() {
    // A save of A and C after one of B and C, interrupted after its first two slots: the later
    // A and C are followed by the earlier save's B:1 and C. (Its B:0 was overwritten by A.)
    (void)::memset(this->m_store, 0, sizeof(this->m_store));
    const U8 valueA[] = {0x10, 0x1D, 0x2A, 0x37};
    const U8 newC[] = {0x60};
    const U8 oldC[] = {0x30};
    const U8 oldB1[] = {1, 2, 3, 4, 5, 6};
    PrmRecord::pack(&this->m_store[0 * PrmRecord::SIZE], RECORD_A, valueA, sizeof(valueA));
    PrmRecord::pack(&this->m_store[1 * PrmRecord::SIZE], RECORD_C, newC, sizeof(newC));
    PrmRecord::pack(&this->m_store[2 * PrmRecord::SIZE], RECORD_B1, oldB1, sizeof(oldB1));
    PrmRecord::pack(&this->m_store[3 * PrmRecord::SIZE], RECORD_C, oldC, sizeof(oldC));

    this->load(StaticPrmDb_LoadMode::REPLACE);
    // Every record is valid; the copy of C in the earlier slot is kept, and B (half loaded) is
    // invalid without counting as incoherent
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::COMPLETE, 4, 0, 0, 0, 1);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x60);
}

void StaticPrmDbTester::testSaveSkipsInvalid() {
    // Fill every slot with a stale record of an earlier save
    const U8 stale[] = {0xEE};
    for (FwSizeType s = 0; s < STORE_RECORDS; s++) {
        PrmRecord::pack(&this->m_store[s * PrmRecord::SIZE], 0x6600 + static_cast<U16>(s), stale, 1);
    }

    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();
    const U16 ids[] = {RECORD_C};
    this->assertStoreRecords(ids, 1);
}

void StaticPrmDbTester::testBlankStore() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    (void)::memset(this->m_store, 0xFF, sizeof(this->m_store));
    this->load(StaticPrmDb_LoadMode::REPLACE);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::COMPLETE, 0, 0, 0, 0, 3);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::INVALID);
}

void StaticPrmDbTester::testReadFailure() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->setParam(PRM_C, SIZE_C, 0x30);
    this->save();

    // Records are saved in parameter order; move C's record into the second transfer so a
    // failure there loads A and B (read by the first transfer) and never reaches C
    const FwSizeType slotC = this->findSlot(RECORD_C);
    ASSERT_LT(slotC, StaticPrmDbConfig::SCRATCH_RECORDS);
    const FwSizeType moved = StaticPrmDbConfig::SCRATCH_RECORDS;
    (void)::memcpy(&this->m_store[moved * PrmRecord::SIZE], &this->m_store[slotC * PrmRecord::SIZE], PrmRecord::SIZE);
    PrmRecord::packEmpty(&this->m_store[slotC * PrmRecord::SIZE]);

    // Values in memory that differ from the store's
    this->setParam(PRM_A, SIZE_A, 0x40);
    this->setParam(PRM_B, SIZE_B, 0x50);
    this->setParam(PRM_C, SIZE_C, 0x60);

    this->m_failTransferNumber = this->m_transfers + 2;
    this->load(StaticPrmDb_LoadMode::REPLACE, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_ReadFailed_SIZE(1);
    ASSERT_EVENTS_ReadFailed(0, Drv::ByteStreamStatus::OTHER_ERROR);
    ASSERT_from_readOut_SIZE(2);
    ASSERT_EQ(s_loadParametersCalls, 1U);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::READ_FAILED, 3, 0, 0, 0, 0);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x20);
    // Not reached: the REPLACE keeps the value in memory instead of invalidating it
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x60);
}

void StaticPrmDbTester::testReadFailureSplitParam() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->setParam(PRM_B, SIZE_B, 0x20);
    this->save();

    // Move B:1 into the second transfer, so a failure there leaves B with only B:0 loaded
    const FwSizeType slotB1 = this->findSlot(RECORD_B1);
    ASSERT_LT(slotB1, StaticPrmDbConfig::SCRATCH_RECORDS);
    const FwSizeType moved = StaticPrmDbConfig::SCRATCH_RECORDS;
    (void)::memcpy(&this->m_store[moved * PrmRecord::SIZE], &this->m_store[slotB1 * PrmRecord::SIZE], PrmRecord::SIZE);
    PrmRecord::packEmpty(&this->m_store[slotB1 * PrmRecord::SIZE]);

    this->setParam(PRM_A, SIZE_A, 0x40);
    this->setParam(PRM_B, SIZE_B, 0x50);
    this->setParam(PRM_C, SIZE_C, 0x60);

    this->m_failTransferNumber = this->m_transfers + 2;
    this->load(StaticPrmDb_LoadMode::REPLACE, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_LoadedDatabase(0, StaticPrmDb_LoadMode::REPLACE, StaticPrmDb_LoadResult::READ_FAILED, 2, 0, 0, 0, 1);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    // B:0 overwrote part of B in memory before the failure, so B can be neither value
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::INVALID);
    this->assertParam(PRM_C, SIZE_C, Fw::ParamValid::VALID, 0x60);

    // The same failure in a MERGE leaves B, valid in memory, untouched
    this->setParam(PRM_B, SIZE_B, 0x50);
    this->m_failTransferNumber = this->m_transfers + 2;
    this->load(StaticPrmDb_LoadMode::MERGE, Fw::CmdResponse::EXECUTION_ERROR);
    this->assertParam(PRM_B, SIZE_B, Fw::ParamValid::VALID, 0x50);
}

void StaticPrmDbTester::testWriteFailure() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->m_failTransferNumber = this->m_transfers + 1;
    this->save(Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_from_writeOut_SIZE(1);
    ASSERT_EVENTS_WriteFailed_SIZE(1);
    ASSERT_EVENTS_WriteFailed(0, Drv::ByteStreamStatus::OTHER_ERROR);
    // The values in memory are untouched
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);

    // Saves may be commanded every second: the failure event is throttled ...
    for (FwSizeType i = 0; i < StaticPrmDbComponentBase::EVENTID_WRITEFAILED_THROTTLE; i++) {
        this->m_failTransferNumber = this->m_transfers + 1;
        this->save(Fw::CmdResponse::EXECUTION_ERROR);
    }
    ASSERT_EVENTS_WriteFailed_SIZE(0);

    // ... a successful save emits no event and re-arms it
    this->save();
    ASSERT_EVENTS_SIZE(0);
    this->m_failTransferNumber = this->m_transfers + 1;
    this->save(Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_WriteFailed_SIZE(1);
}

void StaticPrmDbTester::testBusy() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->clearHistory();
    this->sendCmd_SAVE(TEST_INSTANCE_ID, 1);
    // The command only arms the save; the first transfer is issued by activeIn
    ASSERT_FALSE(this->m_pending.valid);
    ASSERT_TRUE(this->invoke_to_activeIn(0, 0));
    ASSERT_TRUE(this->m_pending.valid);
    // Waiting on storage: no work
    ASSERT_FALSE(this->invoke_to_activeIn(0, 0));

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

    this->run();
    ASSERT_CMD_RESPONSE_SIZE(3);
    ASSERT_CMD_RESPONSE(2, StaticPrmDbComponentBase::OPCODE_SAVE, 1, Fw::CmdResponse::OK);
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
    // Idle: no work
    ASSERT_FALSE(this->invoke_to_activeIn(0, 0));
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

void StaticPrmDbTester::testOneTransferPerCall() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    const FwSizeType transfers =
        (STORE_RECORDS + StaticPrmDbConfig::SCRATCH_RECORDS - 1) / StaticPrmDbConfig::SCRATCH_RECORDS;
    // Storage completing inside the call is only recorded: the next transfer waits for the next
    // activeIn, so no transfer is started from within the storage stack
    this->m_completeInCall = true;
    for (const bool write : {true, false}) {
        this->clearHistory();
        if (write) {
            this->sendCmd_SAVE(TEST_INSTANCE_ID, 1);
        } else {
            this->sendCmd_LOAD(TEST_INSTANCE_ID, 1, StaticPrmDb_LoadMode::REPLACE);
        }
        for (FwSizeType t = 0; t < transfers; t++) {
            ASSERT_TRUE(this->invoke_to_activeIn(0, 0));
            const FwSizeType issued =
                write ? this->fromPortHistory_writeOut->size() : this->fromPortHistory_readOut->size();
            ASSERT_EQ(issued, t + 1) << (write ? "save" : "load");
            ASSERT_CMD_RESPONSE_SIZE(0);
        }
        // The last call acts on the last completion and finishes the command
        ASSERT_TRUE(this->invoke_to_activeIn(0, 0));
        ASSERT_CMD_RESPONSE_SIZE(1);
        ASSERT_CMD_RESPONSE(0, write ? StaticPrmDbComponentBase::OPCODE_SAVE : StaticPrmDbComponentBase::OPCODE_LOAD, 1,
                            Fw::CmdResponse::OK);
        ASSERT_FALSE(this->invoke_to_activeIn(0, 0));
    }
    this->assertParam(PRM_A, SIZE_A, Fw::ParamValid::VALID, 0x10);
}

void StaticPrmDbTester::testChainedCommand() {
    this->setParam(PRM_A, SIZE_A, 0x10);
    this->m_completeInCall = true;
    // The first save fails; its client answers the response with another save
    this->clearHistory();
    this->m_chainSave = true;
    this->m_failTransferNumber = this->m_transfers + 1;
    this->sendCmd_SAVE(TEST_INSTANCE_ID, 1);
    this->run();
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, StaticPrmDbComponentBase::OPCODE_SAVE, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_CMD_RESPONSE(1, StaticPrmDbComponentBase::OPCODE_SAVE, CHAINED_CMD_SEQ, Fw::CmdResponse::OK);
    // One failed transfer, then the second save's transfers, one at a time (the simulated store
    // checks that no transfer is issued while another is outstanding)
    const FwSizeType transfers =
        (STORE_RECORDS + StaticPrmDbConfig::SCRATCH_RECORDS - 1) / StaticPrmDbConfig::SCRATCH_RECORDS;
    ASSERT_EQ(this->fromPortHistory_writeOut->size(), transfers + 1);
}

void StaticPrmDbTester::testNoStorage() {
    // A deployment that does not connect persistent storage
    StaticPrmDb unconnected("Unconnected");
    unconnected.init(TEST_INSTANCE_ID + 1);
    this->connect_to_cmdIn(0, unconnected.get_cmdIn_InputPort(0));
    unconnected.set_cmdResponseOut_OutputPort(0, this->get_from_cmdResponseOut(0));
    unconnected.set_logOut_OutputPort(0, this->get_from_logOut(0));
    unconnected.set_timeCaller_OutputPort(0, this->get_from_timeCaller(0));

    this->clearHistory();
    this->sendCmd_SAVE(TEST_INSTANCE_ID, 1);
    this->sendCmd_LOAD(TEST_INSTANCE_ID, 2, StaticPrmDb_LoadMode::REPLACE);
    ASSERT_CMD_RESPONSE_SIZE(2);
    ASSERT_CMD_RESPONSE(0, StaticPrmDbComponentBase::OPCODE_SAVE, 1, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_CMD_RESPONSE(1, StaticPrmDbComponentBase::OPCODE_LOAD, 2, Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_NoStorage_SIZE(2);
    ASSERT_EVENTS_NoStorage(0, StaticPrmDb_IoState::WRITING);
    ASSERT_EVENTS_NoStorage(1, StaticPrmDb_IoState::READING);
    ASSERT_FALSE(unconnected.isConnected_readOut_OutputPort(0));
}

}  // namespace Samd21

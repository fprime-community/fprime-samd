// ======================================================================
// \title  StaticPrmDb.cpp
// \author tumbar
// \brief  cpp file for StaticPrmDb component implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDb.hpp"
#include "Fw/Cmd/CmdResponseEnumAc.hpp"
#include "Fw/Prm/ParamValidEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"

#include <cstring>

namespace Samd21 {

// Definitions of the ODR-used class constants (C++14)
constexpr FwSizeType StaticPrmDb::SCRATCH_SIZE;
constexpr FwSizeType StaticPrmDb::STORE_SIZE;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StaticPrmDb::StaticPrmDb(const char* const compName)
    : StaticPrmDbComponentBase(compName),
      m_ioState(StaticPrmDb_IoState::IDLE),
      m_opCode(0),
      m_cmdSeq(0),
      m_loadMode(StaticPrmDb_LoadMode::REPLACE),
      m_transfer(Transfer::NONE),
      m_transferStatus(Drv::ByteStreamStatus::OP_OK),
      m_transferRecords(0),
      m_slot(0),
      m_record(0),
      m_validRecords(0),
      m_corruptRecords(0),
      m_unknownRecords(0),
      m_scratch() {}

StaticPrmDb::~StaticPrmDb() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::ParamValid StaticPrmDb::getPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    if (this->m_ioState != StaticPrmDb_IoState::IDLE) {
        // DB is busy, we should not be reading from it
        return Fw::ParamValid::INVALID;
    }
    return StaticPrmDb::readParam(id, val);
}

void StaticPrmDb::setPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    if (this->m_ioState != StaticPrmDb_IoState::IDLE) {
        this->log_WARNING_HI_DroppingPrmSave(id, this->m_ioState);
        return;
    }
    StaticPrmDb::writeParam(id, val);
}

bool StaticPrmDb::activeIn_handler(FwIndexType portNum, U32 context) {
    if ((this->m_ioState == StaticPrmDb_IoState::IDLE) || (this->m_transfer == Transfer::OUTSTANDING)) {
        // Nothing to do until a command starts a transfer or storage completes one
        return false;
    }

    if (this->m_transfer == Transfer::COMPLETED) {
        // May finish the command, and the command response may start another one
        this->transferCompleted();
    }

    // One transfer per call: its completion is acted on by the next call
    if ((this->m_ioState != StaticPrmDb_IoState::IDLE) && (this->m_transfer == Transfer::NONE)) {
        if (this->m_ioState == StaticPrmDb_IoState::WRITING) {
            this->saveNext();
        } else {
            this->loadNext();
        }
    }
    return true;
}

void StaticPrmDb::readReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const Drv::ByteStreamStatus& status) {
    // Only record the completion: activeIn acts on it, so no transfer is started from inside
    // the storage stack that completed this one
    this->m_transferRecords = this->transferReturned(fwBuffer, StaticPrmDb_IoState::READING);
    this->m_transferStatus = status;
    this->m_transfer = Transfer::COMPLETED;
}

void StaticPrmDb::writeReturnIn_handler(FwIndexType portNum,
                                        Fw::Buffer& fwBuffer,
                                        const Drv::ByteStreamStatus& status) {
    this->m_transferRecords = this->transferReturned(fwBuffer, StaticPrmDb_IoState::WRITING);
    this->m_transferStatus = status;
    this->m_transfer = Transfer::COMPLETED;
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticPrmDb::SAVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    if (!this->startCommand(opCode, cmdSeq, StaticPrmDb_IoState::WRITING)) {
        return;
    }
    this->m_record = 0;
}

void StaticPrmDb::LOAD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const StaticPrmDb_LoadMode& mode) {
    // The command dispatch rejects an out-of-range mode with FORMAT_ERROR before this handler
    FW_ASSERT(mode.isValid(), static_cast<FwAssertArgType>(mode.e));
    if (!this->startCommand(opCode, cmdSeq, StaticPrmDb_IoState::READING)) {
        return;
    }

    // Validity only changes when the load is committed, so a failed read leaves every parameter
    // it did not reach as it was (REPLACE invalidates parameters missing from the store then)
    StaticPrmDb::beginLoad();
    this->m_loadMode = mode;
    this->m_validRecords = 0;
    this->m_corruptRecords = 0;
    this->m_unknownRecords = 0;
}

// ----------------------------------------------------------------------
// Transfers
// ----------------------------------------------------------------------

bool StaticPrmDb::startCommand(FwOpcodeType opCode, U32 cmdSeq, StaticPrmDb_IoState::T state) {
    if (this->m_ioState != StaticPrmDb_IoState::IDLE) {
        // A transaction is in progress. Reject this command
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return false;
    }
    FW_ASSERT(this->m_transfer == Transfer::NONE, static_cast<FwAssertArgType>(this->m_transfer));

    const bool connected = (state == StaticPrmDb_IoState::WRITING) ? this->isConnected_writeOut_OutputPort(0)
                                                                   : this->isConnected_readOut_OutputPort(0);
    if (!connected) {
        // A deployment without persistent storage: fail the command rather than the port call
        this->log_WARNING_HI_NoStorage(state);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
        return false;
    }

    // The first transfer is issued by the next activeIn
    this->m_ioState = state;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->m_slot = 0;
    return true;
}

void StaticPrmDb::transferCompleted() {
    FW_ASSERT(this->m_transfer == Transfer::COMPLETED, static_cast<FwAssertArgType>(this->m_transfer));
    FW_ASSERT(this->m_slot + this->m_transferRecords <= StaticPrmDbConfig::STORE_RECORDS,
              static_cast<FwAssertArgType>(this->m_slot), static_cast<FwAssertArgType>(this->m_transferRecords));
    this->m_transfer = Transfer::NONE;
    const bool ok = (this->m_transferStatus == Drv::ByteStreamStatus::OP_OK);

    if (this->m_ioState == StaticPrmDb_IoState::READING) {
        if (!ok) {
            // Apply what was read so far and keep the rest: a MERGE of the redundant store fills
            // in whatever is left invalid
            this->log_WARNING_HI_ReadFailed(this->m_transferStatus);
            this->finishLoad(StaticPrmDb_LoadResult::READ_FAILED);
            return;
        }
        this->unpackScratch(this->m_transferRecords);
        this->m_slot += this->m_transferRecords;
        if (this->m_slot == StaticPrmDbConfig::STORE_RECORDS) {
            this->finishLoad(StaticPrmDb_LoadResult::COMPLETE);
        }
        return;
    }

    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::WRITING, static_cast<FwAssertArgType>(this->m_ioState));
    if (!ok) {
        // Throttled: a save may be commanded every second
        this->log_WARNING_HI_WriteFailed(this->m_transferStatus);
        this->finish(Fw::CmdResponse::EXECUTION_ERROR);
        return;
    }
    this->m_slot += this->m_transferRecords;
    if (this->m_slot == StaticPrmDbConfig::STORE_RECORDS) {
        // A successful save emits no event; it re-arms the failure event instead
        this->log_WARNING_HI_WriteFailed_ThrottleClear();
        this->finish(Fw::CmdResponse::OK);
    }
}

void StaticPrmDb::saveNext() {
    FW_ASSERT(this->m_slot < StaticPrmDbConfig::STORE_RECORDS, static_cast<FwAssertArgType>(this->m_slot));
    // Guaranteed by the generated code (static_assert NUM_RECORDS <= STORE_RECORDS)
    FW_ASSERT(StaticPrmDb::numRecords() <= StaticPrmDbConfig::STORE_RECORDS,
              static_cast<FwAssertArgType>(StaticPrmDb::numRecords()));

    const FwSizeType left = StaticPrmDbConfig::STORE_RECORDS - this->m_slot;
    const FwSizeType slots = (left < StaticPrmDbConfig::SCRATCH_RECORDS) ? left : StaticPrmDbConfig::SCRATCH_RECORDS;

    // Each pass either fills a slot or skips a record of an invalid parameter (which takes no
    // slot), so the transfer is packed in at most slots + numRecords() passes
    constexpr FwSizeType MAX_PASSES = StaticPrmDbConfig::SCRATCH_RECORDS + StaticPrmDbConfig::STORE_RECORDS;
    FwSizeType records = 0;
    for (FwSizeType pass = 0; (pass < MAX_PASSES) && (records < slots); pass++) {
        U8* record = &this->m_scratch[records * PrmRecord::SIZE];
        if (this->m_record < StaticPrmDb::numRecords()) {
            // A record of an invalid parameter takes no slot
            const bool packed = StaticPrmDb::saveRecord(this->m_record, record);
            this->m_record++;
            if (packed) {
                records++;
            }
        } else {
            // Clear the rest of the store so records of an earlier save are not loaded back
            PrmRecord::packEmpty(record);
            records++;
        }
    }
    FW_ASSERT(records == slots, static_cast<FwAssertArgType>(records), static_cast<FwAssertArgType>(slots));

    this->m_transferRecords = records;
    this->m_transfer = Transfer::OUTSTANDING;
    Fw::Buffer buffer(this->m_scratch, records * PrmRecord::SIZE);
    this->writeOut_out(0, static_cast<U32>(this->m_slot * PrmRecord::SIZE), buffer);
}

void StaticPrmDb::loadNext() {
    FW_ASSERT(this->m_slot < StaticPrmDbConfig::STORE_RECORDS, static_cast<FwAssertArgType>(this->m_slot));

    const FwSizeType left = StaticPrmDbConfig::STORE_RECORDS - this->m_slot;
    const FwSizeType records = (left < StaticPrmDbConfig::SCRATCH_RECORDS) ? left : StaticPrmDbConfig::SCRATCH_RECORDS;
    this->m_transferRecords = records;
    this->m_transfer = Transfer::OUTSTANDING;
    Fw::Buffer buffer(this->m_scratch, records * PrmRecord::SIZE);
    this->readOut_out(0, static_cast<U32>(this->m_slot * PrmRecord::SIZE), buffer);
}

void StaticPrmDb::unpackScratch(FwSizeType records) {
    FW_ASSERT(records <= StaticPrmDbConfig::SCRATCH_RECORDS, static_cast<FwAssertArgType>(records));
    const bool merge = (this->m_loadMode == StaticPrmDb_LoadMode::MERGE);
    for (FwSizeType i = 0; i < records; i++) {
        const U8* record = &this->m_scratch[i * PrmRecord::SIZE];
        if (PrmRecord::isEmpty(record)) {
            continue;
        }
        if (!PrmRecord::crcValid(record)) {
            this->m_corruptRecords++;
            continue;
        }
        if (StaticPrmDb::loadRecord(record, merge) == RecordLoad::UNKNOWN) {
            this->m_unknownRecords++;
        } else {
            this->m_validRecords++;
        }
    }
}

void StaticPrmDb::finishLoad(StaticPrmDb_LoadResult result) {
    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::READING, static_cast<FwAssertArgType>(this->m_ioState));
    const bool complete = (result == StaticPrmDb_LoadResult::COMPLETE);

    // Segments of different saves (an interrupted SAVE) or a partly loaded value leave the
    // parameter invalid, so a MERGE of the redundant store fills it in whole
    const U32 incoherentParams = StaticPrmDb::commitLoad(complete, this->m_loadMode == StaticPrmDb_LoadMode::MERGE);
    const U32 invalidParams = StaticPrmDb::countInvalidParams();
    this->tlmWrite_ValidRecords(this->m_validRecords);
    this->tlmWrite_CorruptRecords(this->m_corruptRecords);
    this->tlmWrite_InvalidParameters(invalidParams);
    this->tlmWrite_IncoherentParameters(incoherentParams);
    this->log_ACTIVITY_HI_LoadedDatabase(this->m_loadMode, result, this->m_validRecords, this->m_corruptRecords,
                                         this->m_unknownRecords, incoherentParams, invalidParams);

    // Idle first: the components read their parameters back through getPrm
    this->m_ioState = StaticPrmDb_IoState::IDLE;

    // Percolate the parameters to the rest of the system
    StaticPrmDb::loadParameters();

    this->finish(complete ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

void StaticPrmDb::finish(Fw::CmdResponse response) {
    FW_ASSERT(this->m_transfer == Transfer::NONE, static_cast<FwAssertArgType>(this->m_transfer));
    this->m_ioState = StaticPrmDb_IoState::IDLE;
    this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, response);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

FwSizeType StaticPrmDb::transferReturned(const Fw::Buffer& buffer, StaticPrmDb_IoState::T state) const {
    // Exactly one completion follows each transfer, of the kind issued, with the scratch buffer
    // sent out: anything else is a breach of the storage contract, a programming error
    FW_ASSERT(this->m_ioState == state, static_cast<FwAssertArgType>(this->m_ioState));
    FW_ASSERT(this->m_transfer == Transfer::OUTSTANDING, static_cast<FwAssertArgType>(this->m_transfer));
    FW_ASSERT(buffer.getData() == this->m_scratch);
    FW_ASSERT(buffer.getSize() == this->m_transferRecords * PrmRecord::SIZE,
              static_cast<FwAssertArgType>(buffer.getSize()));
    return this->m_transferRecords;
}

// ----------------------------------------------------------------------
// Shared helpers of the generated code
// ----------------------------------------------------------------------

Fw::ParamValid StaticPrmDb::readValue(const ParamLayout& layout, Fw::ParamBuffer& val) {
    if ((*layout.validity & layout.mask) != layout.mask) {
        return Fw::ParamValid::INVALID;
    }
    const Fw::SerializeStatus status = val.setBuff(layout.value, layout.size);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    return Fw::ParamValid::VALID;
}

void StaticPrmDb::writeValue(const ParamLayout& layout, const Fw::ParamBuffer& val) {
    FW_ASSERT(val.getSize() == layout.size, static_cast<FwAssertArgType>(val.getSize()),
              static_cast<FwAssertArgType>(layout.size));
    (void)::memcpy(layout.value, val.getBuffAddr(), static_cast<size_t>(layout.size));
    if (layout.check != nullptr) {
        *layout.check = PrmRecord::valueCheck(layout.value, layout.size);
    }
    *layout.validity |= layout.mask;
}

StaticPrmDb::RecordLoad StaticPrmDb::loadSegment(const U8* record,
                                                 bool merge,
                                                 const ParamLayout& layout,
                                                 FwSizeType segment) {
    FW_ASSERT(segment < PrmRecord::recordCount(layout.size), static_cast<FwAssertArgType>(segment));
    const U32 bit = static_cast<U32>(1U << (layout.firstBit + segment));
    FW_ASSERT((bit & layout.mask) != 0, static_cast<FwAssertArgType>(bit));

    // The first copy loaded wins: a SAVE writes from slot 0 up, so in an interrupted save the
    // newer copy is first. A MERGE keeps a parameter already whole from the earlier store.
    const U32 kept = merge ? (*layout.loaded | *layout.validity) : *layout.loaded;
    if ((kept & bit) != 0) {
        return RecordLoad::KEPT;
    }

    // The segment holds stored bytes [start, end) of the value followed by its value check
    const FwSizeType start = segment * PrmRecord::VALUE_SIZE;
    const FwSizeType stored = PrmRecord::storedSize(layout.size);
    const FwSizeType end = ((start + PrmRecord::VALUE_SIZE) < stored) ? (start + PrmRecord::VALUE_SIZE) : stored;
    const U8* bytes = &record[PrmRecord::VALUE_OFFSET];
    const FwSizeType valueEnd = (end < layout.size) ? end : layout.size;
    if (start < valueEnd) {
        (void)::memcpy(&layout.value[start], bytes, static_cast<size_t>(valueEnd - start));
    }
    if (end > layout.size) {
        FW_ASSERT(layout.check != nullptr);
        const FwSizeType checkStart = (start > layout.size) ? (start - layout.size) : 0;
        const FwSizeType checkLength = end - ((start > layout.size) ? start : layout.size);
        // Update only this segment's bytes of the check, in its in-memory byte order
        U8 check[PrmRecord::CHECK_SIZE];
        (void)::memcpy(check, layout.check, sizeof(check));
        (void)::memcpy(&check[checkStart], &bytes[valueEnd - start], static_cast<size_t>(checkLength));
        (void)::memcpy(layout.check, check, sizeof(check));
    }
    *layout.loaded |= bit;
    return RecordLoad::LOADED;
}

bool StaticPrmDb::saveSegment(U8* record, U16 recordId, const ParamLayout& layout, FwSizeType segment) {
    FW_ASSERT(segment < PrmRecord::recordCount(layout.size), static_cast<FwAssertArgType>(segment));
    // Parameters without a valid value are left out: a load leaves them invalid
    if ((*layout.validity & layout.mask) != layout.mask) {
        return false;
    }

    const FwSizeType start = segment * PrmRecord::VALUE_SIZE;
    const FwSizeType stored = PrmRecord::storedSize(layout.size);
    const FwSizeType end = ((start + PrmRecord::VALUE_SIZE) < stored) ? (start + PrmRecord::VALUE_SIZE) : stored;
    const FwSizeType valueEnd = (end < layout.size) ? end : layout.size;
    const FwSizeType valueLength = (start < valueEnd) ? (valueEnd - start) : 0;
    U8 check[PrmRecord::CHECK_SIZE] = {};
    FwSizeType checkStart = 0;
    FwSizeType checkLength = 0;
    if (end > layout.size) {
        FW_ASSERT(layout.check != nullptr);
        (void)::memcpy(check, layout.check, sizeof(check));
        checkStart = (start > layout.size) ? (start - layout.size) : 0;
        checkLength = end - ((start > layout.size) ? start : layout.size);
    }
    PrmRecord::pack(record, recordId, &layout.value[(valueLength > 0) ? start : 0], valueLength, &check[checkStart],
                    checkLength);
    return true;
}

U32 StaticPrmDb::commitValue(bool complete, bool merge, const ParamLayout& layout) {
    const U32 loaded = *layout.loaded & layout.mask;
    if (loaded == 0) {
        // Its value in memory is untouched. A REPLACE that read the whole store did not find it;
        // a failed load or a MERGE keeps it as it was.
        if (complete && !merge) {
            *layout.validity &= ~layout.mask;
        }
        return 0;
    }
    if (loaded != layout.mask) {
        // Partly overwritten: a segment corrupt, unknown or (after a failed read) not read
        *layout.validity &= ~layout.mask;
        return 0;
    }
    if ((layout.check != nullptr) && (PrmRecord::valueCheck(layout.value, layout.size) != *layout.check)) {
        // Every segment loaded, but not all of the same save
        *layout.validity &= ~layout.mask;
        return 1;
    }
    *layout.validity |= layout.mask;
    return 0;
}

}  // namespace Samd21

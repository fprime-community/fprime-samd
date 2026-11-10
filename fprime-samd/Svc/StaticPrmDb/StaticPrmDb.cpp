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
      m_inCall(false),
      m_completedInCall(false),
      m_slot(0),
      m_record(0),
      m_skippedParams(0),
      m_validRecords(0),
      m_corruptRecords(0),
      m_unknownRecords(0),
      m_scratch() {}

StaticPrmDb::~StaticPrmDb() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Fw::ParamValid StaticPrmDb::getPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    if (this->m_ioState == StaticPrmDb_IoState::IDLE) {
        return this->readParam(id, val);
    } else {
        // DB is busy, we should not be reading from it
        return Fw::ParamValid::INVALID;
    }
}

void StaticPrmDb::setPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    if (this->m_ioState == StaticPrmDb_IoState::IDLE) {
        this->writeParam(id, val);
    } else {
        this->log_WARNING_HI_DroppingPrmSave(id, this->m_ioState);
    }
}

void StaticPrmDb::readReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const Drv::ByteStreamStatus& status) {
    // Make sure we are waiting for a reply
    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::READING, static_cast<FwAssertArgType>(this->m_ioState));

    const FwSizeType records = scratchRecords(fwBuffer);

    if (status == Drv::ByteStreamStatus::OP_OK) {
        this->unpackScratch(records);
        this->m_slot += records;
    } else {
        // Keep what was read so far: a MERGE load of the redundant store fills in the rest
        this->log_WARNING_HI_ReadFailed(status);
        this->finishLoad(Fw::CmdResponse::EXECUTION_ERROR);
    }
    this->transferDone();
}

void StaticPrmDb::writeReturnIn_handler(FwIndexType portNum,
                                        Fw::Buffer& fwBuffer,
                                        const Drv::ByteStreamStatus& status) {
    // Make sure we are waiting for a reply
    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::WRITING, static_cast<FwAssertArgType>(this->m_ioState));

    const FwSizeType records = scratchRecords(fwBuffer);

    if (status == Drv::ByteStreamStatus::OP_OK) {
        this->m_slot += records;
    } else {
        // We failed to write the data down to the bank
        this->finish(Fw::CmdResponse::EXECUTION_ERROR);
    }
    this->transferDone();
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticPrmDb::SAVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Make sure there is nothing in progress
    if (this->m_ioState != StaticPrmDb_IoState::IDLE) {
        // A transaction is in progress. Reject this command
        return this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
    }

    this->m_ioState = StaticPrmDb_IoState::WRITING;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->m_slot = 0;
    this->m_record = 0;
    // Invalid parameters are left out of the store; they can't change during the save
    this->m_skippedParams = this->countInvalidParams();
    this->transferLoop();
}

void StaticPrmDb::LOAD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const StaticPrmDb_LoadMode& mode) {
    // Make sure there is nothing in progress
    if (this->m_ioState != StaticPrmDb_IoState::IDLE) {
        // A transaction is in progress. Reject this command
        return this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
    }

    if (mode == StaticPrmDb_LoadMode::REPLACE) {
        // Every value has to come from the store
        this->invalidateAll();
    }

    this->m_ioState = StaticPrmDb_IoState::READING;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->m_loadMode = mode;
    this->m_slot = 0;
    this->m_validRecords = 0;
    this->m_corruptRecords = 0;
    this->m_unknownRecords = 0;
    this->transferLoop();
}

// ----------------------------------------------------------------------
// Transfers
// ----------------------------------------------------------------------

void StaticPrmDb::transferLoop() {
    // Storage may complete a transfer inside the call that started it. Each such completion
    // continues here rather than starting the next transfer from the completion handler, so the
    // stack stays flat however many transfers the store takes.
    while (this->m_ioState != StaticPrmDb_IoState::IDLE) {
        this->m_inCall = true;
        this->m_completedInCall = false;
        if (this->m_ioState == StaticPrmDb_IoState::WRITING) {
            this->saveNext();
        } else {
            this->loadNext();
        }
        this->m_inCall = false;
        if (!this->m_completedInCall) {
            // The completion is asynchronous (or the command finished), it continues the loop
            return;
        }
    }
}

void StaticPrmDb::transferDone() {
    if (this->m_inCall) {
        this->m_completedInCall = true;
    } else {
        this->transferLoop();
    }
}

void StaticPrmDb::saveNext() {
    const FwSizeType slots = StaticPrmDbConfig::STORE_RECORDS;
    if (this->m_slot >= slots) {
        this->finish(Fw::CmdResponse::OK);
        return;
    }

    FwSizeType records = 0;
    while ((records < StaticPrmDbConfig::SCRATCH_RECORDS) && (this->m_slot + records < slots)) {
        U8* record = &this->m_scratch[records * PrmRecord::SIZE];
        if (this->m_record < this->numRecords()) {
            // A record of an invalid parameter takes no slot
            if (this->saveRecord(this->m_record++, record)) {
                records++;
            }
        } else {
            // Clear the rest of the store so records of an earlier save are not loaded back
            PrmRecord::packEmpty(record);
            records++;
        }
    }

    Fw::Buffer buffer(this->m_scratch, records * PrmRecord::SIZE);
    this->writeOut_out(0, static_cast<U32>(this->m_slot * PrmRecord::SIZE), buffer);
}

void StaticPrmDb::loadNext() {
    if (this->m_slot >= StaticPrmDbConfig::STORE_RECORDS) {
        this->finishLoad(Fw::CmdResponse::OK);
        return;
    }

    const FwSizeType left = StaticPrmDbConfig::STORE_RECORDS - this->m_slot;
    const FwSizeType records = (left < StaticPrmDbConfig::SCRATCH_RECORDS) ? left : StaticPrmDbConfig::SCRATCH_RECORDS;
    Fw::Buffer buffer(this->m_scratch, records * PrmRecord::SIZE);
    this->readOut_out(0, static_cast<U32>(this->m_slot * PrmRecord::SIZE), buffer);
}

void StaticPrmDb::unpackScratch(FwSizeType records) {
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
        if (this->loadRecord(record, merge) == RecordLoad::UNKNOWN) {
            this->m_unknownRecords++;
        } else {
            this->m_validRecords++;
        }
    }
}

void StaticPrmDb::finishLoad(Fw::CmdResponse response) {
    const U32 invalidParams = countInvalidParams();
    this->tlmWrite_ValidRecords(this->m_validRecords);
    this->tlmWrite_CorruptRecords(this->m_corruptRecords);
    this->tlmWrite_InvalidParameters(invalidParams);
    this->log_ACTIVITY_HI_LoadedDatabase(this->m_loadMode, this->m_validRecords, this->m_corruptRecords,
                                         this->m_unknownRecords, invalidParams);

    // Idle first: the components read their parameters back through getPrm
    this->m_ioState = StaticPrmDb_IoState::IDLE;

    // Percolate the parameters to the rest of the system
    this->loadParameters();

    this->finish(response);
}

void StaticPrmDb::finish(Fw::CmdResponse response) {
    this->m_ioState = StaticPrmDb_IoState::IDLE;
    this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, response);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

FwSizeType StaticPrmDb::scratchRecords(const Fw::Buffer& buffer) const {
    // We should be getting back the scratch buffer we sent out
    FW_ASSERT(buffer.getData() == this->m_scratch);
    FW_ASSERT(buffer.getSize() % PrmRecord::SIZE == 0, static_cast<FwAssertArgType>(buffer.getSize()));
    FW_ASSERT((buffer.getSize() > 0) && (buffer.getSize() <= SCRATCH_SIZE),
              static_cast<FwAssertArgType>(buffer.getSize()));
    return buffer.getSize() / PrmRecord::SIZE;
}

}  // namespace Samd21

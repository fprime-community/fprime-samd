// ======================================================================
// \title  PrmStorage.cpp
// \author tumbar
// \brief  cpp file for PrmStorage component implementation class
// ======================================================================

#include "fprime-samd/Svc/PrmStorage/PrmStorage.hpp"
#include "Fw/Types/Assert.hpp"
#include "fprime-samd/Svc/PrmStorage/PrmStorageLayout.hpp"
#include "fprime-samd/Svc/Types/PrmRecord.hpp"
#include "samd-config/PrmStorageConfig.hpp"
#include "samd-config/StaticPrmDbConfig.hpp"

namespace Samd21 {

// Both banks hold the StaticPrmDb store inside the F-RAM without overlapping
static_assert(PrmStorageLayout::banksHold(StaticPrmDbConfig::STORE_RECORDS* PrmRecord::SIZE),
              "The parameter store (StaticPrmDbConfig::STORE_RECORDS records) does not fit the PrmStorage banks");

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PrmStorage::PrmStorage(const char* const compName)
    : PrmStorageComponentBase(compName),
      m_bank(PrmBank::PRIME),
      m_busy(false),
      m_retryPending(false),
      m_op(Fram::Op::READ),
      m_offset(0),
      m_buffer(),
      m_cursor(0),
      m_chunk(),
      m_retries(0) {}

PrmStorage::~PrmStorage() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PrmStorage::writeIn_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->start(Fram::Op::WRITE, offset, buffer);
}

void PrmStorage::readIn_handler(FwIndexType portNum, U32 offset, Fw::Buffer& buffer) {
    this->start(Fram::Op::READ, offset, buffer);
}

void PrmStorage::framReadComplete_handler(FwIndexType portNum,
                                          U32 offset,
                                          Fw::Buffer& buffer,
                                          const Fram::Status& status) {
    this->complete(Fram::Op::READ, status);
}

void PrmStorage::framWriteComplete_handler(FwIndexType portNum,
                                           U32 offset,
                                           Fw::Buffer& buffer,
                                           const Fram::Status& status) {
    this->complete(Fram::Op::WRITE, status);
}

void PrmStorage::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_retryPending) {
        this->m_retryPending = false;
        this->send();
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void PrmStorage::SELECT_BANK_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Samd21::PrmBank& bank) {
    if (this->m_busy) {
        // The transfer in progress keeps the bank it started on
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }
    this->m_bank = bank;
    this->tlmWrite_ActiveBank(bank);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void PrmStorage::start(Fram::Op op, U32 offset, Fw::Buffer& buffer) {
    // StaticPrmDb holds one transaction at a time
    FW_ASSERT(!this->m_busy, static_cast<FwAssertArgType>(op));
    this->m_busy = true;
    this->m_op = op;
    this->m_offset = offset;
    this->m_buffer = buffer;
    this->m_cursor = 0;
    this->nextChunk();
}

void PrmStorage::nextChunk() {
    const FwSizeType left = this->m_buffer.getSize() - this->m_cursor;
    if (left == 0) {
        this->finish(Drv::ByteStreamStatus::OP_OK);
        return;
    }
    const FwSizeType size = (left < static_cast<FwSizeType>(Fram::FramConfig::MaxTransfer))
                                ? left
                                : static_cast<FwSizeType>(Fram::FramConfig::MaxTransfer);
    this->m_chunk = Fw::Buffer(this->m_buffer.getData() + this->m_cursor, size);
    this->m_retries = 0;
    this->send();
}

void PrmStorage::complete(Fram::Op op, Fram::Status status) {
    // Exactly one completion follows an accepted request (Fram.Request contract)
    FW_ASSERT(this->m_busy, static_cast<FwAssertArgType>(op));
    FW_ASSERT(op == this->m_op, static_cast<FwAssertArgType>(op));
    if (status != Fram::Status::OK) {
        this->fail(status);
        return;
    }
    this->m_cursor += this->m_chunk.getSize();
    this->nextChunk();
}

void PrmStorage::send() {
    const U32 offset = this->bankOffset() + this->m_offset + static_cast<U32>(this->m_cursor);
    // Copy the view: the driver may complete the request, and this component
    // issue the next one, before the call returns
    Fw::Buffer chunk = this->m_chunk;
    const Fram::Status status =
        (this->m_op == Fram::Op::READ) ? this->framRead_out(0, offset, chunk) : this->framWrite_out(0, offset, chunk);
    if (status == Fram::Status::ACCEPTED) {
        return;
    }

    if ((status == Fram::Status::BUSY) && (this->m_retries < PrmStorageConfig::BUSY_RETRY_LIMIT)) {
        // The driver is running its startup reads or another client's transfer
        this->m_retries++;
        this->m_retryPending = true;
        return;
    }
    // Rejected: no completion follows, so act on it here
    this->fail(status);
}

void PrmStorage::fail(Fram::Status status) {
    const U32 offset = this->bankOffset() + this->m_offset + static_cast<U32>(this->m_cursor);
    this->log_WARNING_HI_StorageFailed(this->m_op, this->m_bank, offset, status);
    this->finish(Drv::ByteStreamStatus::OTHER_ERROR);
}

void PrmStorage::finish(Drv::ByteStreamStatus status) {
    // Idle first: the database may start its next transfer from the return
    this->m_busy = false;
    this->m_retryPending = false;
    if (this->m_op == Fram::Op::READ) {
        this->readReturnOut_out(0, this->m_buffer, status);
    } else {
        this->writeReturnOut_out(0, this->m_buffer, status);
    }
}

U32 PrmStorage::bankOffset() const {
    return (this->m_bank == PrmBank::BACKUP) ? PrmStorageConfig::BACKUP_OFFSET : PrmStorageConfig::PRIME_OFFSET;
}

}  // namespace Samd21

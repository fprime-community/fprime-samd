// ======================================================================
// \title  StaticPrmDb.cpp
// \author tumbar
// \brief  cpp file for StaticPrmDb component implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDb.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Fw/Cmd/CmdResponseEnumAc.hpp"
#include "Fw/Prm/ParamValidEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/SuccessEnumAc.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDb_IoStateEnumAc.hpp"
#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDb_PrmBankEnumAc.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StaticPrmDb::StaticPrmDb(const char* const compName)
    : StaticPrmDbComponentBase(compName),
      m_ioState(StaticPrmDb_IoState::IDLE),
      m_ioBank(StaticPrmDb_PrmBank::PRIME),
      m_opCode(0),
      m_cmdSeq(0) {
    this->initializeDefaults();
}

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

void StaticPrmDb::readReturnIn_handler(FwIndexType portNum, Fw::Buffer& fwBuffer, const Drv::ByteStreamStatus& status) {
    // Make sure we are waiting for a reply
    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::READING, static_cast<FwAssertArgType>(this->m_ioState));

    // Make sure we are waiting for the proper bank to reply
    FW_ASSERT(portNum == this->m_ioBank, portNum, this->m_ioBank);

    // We should be getting back the same buffer we sent out
    auto txBuf = this->getDatabaseBuffer();
    FW_ASSERT(fwBuffer.getData() == txBuf.getData());

    Fw::Success result;

    if (status == Drv::ByteStreamStatus::OP_OK) {
        Fw::ExternalSerializeBuffer txBufSer(txBuf.getData(), txBuf.getSize());
        U32 storedHash;
        auto serStatus = txBufSer.deserializeTo(storedHash);
        FW_ASSERT(serStatus == Fw::FW_SERIALIZE_OK, serStatus);

        U32 generatedHash = this->getDeploymentParameterHash();

        if (storedHash == generatedHash) {
            U32 validRecords;
            U32 invalidRecords;
            this->validate(validRecords, invalidRecords);

            this->log_ACTIVITY_HI_LoadedDatabase(this->m_ioBank, validRecords, invalidRecords);

            // Percolate the parameters to the rest of the system
            this->loadParameters();

            result = Fw::Success::SUCCESS;
        } else {
            result = Fw::Success::FAILURE;
            this->log_WARNING_HI_MismatchedParameterDatabaseHash(this->m_ioBank, storedHash, generatedHash);
        }
    } else {
        result = Fw::Success::FAILURE;
        this->log_WARNING_HI_ReadFailed(this->m_ioBank, status);
    }

    if (result == Fw::Success::SUCCESS) {
        this->m_ioState = StaticPrmDb_IoState::IDLE;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::OK);
    } else {
        // Clear the in-memory database since the entire thing is invalid
        this->initializeDefaults();

        this->m_ioState = StaticPrmDb_IoState::IDLE;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void StaticPrmDb::setPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    if (this->m_ioState == StaticPrmDb_IoState::IDLE) {
        auto status = this->writeParam(id, val);
        FW_ASSERT(status == Fw::Success::SUCCESS, static_cast<FwAssertArgType>(status),
                  static_cast<FwAssertArgType>(id));
    } else {
        this->log_WARNING_HI_DroppingPrmSave(id, this->m_ioState);
    }
}

void StaticPrmDb::writeReturnIn_handler(FwIndexType portNum,
                                        Fw::Buffer& fwBuffer,
                                        const Drv::ByteStreamStatus& status) {
    // Make sure we are waiting for a reply
    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::WRITING, static_cast<FwAssertArgType>(this->m_ioState));

    // Make sure we are waiting for the proper bank to reply
    FW_ASSERT(portNum == this->m_ioBank, portNum, this->m_ioBank);

    // We should be getting back the same buffer we sent out
    auto txBuf = this->getDatabaseBuffer();
    FW_ASSERT(fwBuffer.getData() == txBuf.getData());

    if (status == Drv::ByteStreamStatus::OP_OK) {
        this->m_ioState = StaticPrmDb_IoState::IDLE;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::OK);
    } else {
        // We failed to write the data down to the bank
        this->m_ioState = StaticPrmDb_IoState::IDLE;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticPrmDb::SAVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Samd21::StaticPrmDb_PrmBank& bank) {
    // Make sure there is nothing in progress
    switch (this->m_ioState) {
        case StaticPrmDb_IoState::IDLE:  // This is OK
            break;
        case StaticPrmDb_IoState::READING:
        case StaticPrmDb_IoState::WRITING:
            // A transaction is in progress. Reject this command
            return this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
    }

    this->m_ioState = StaticPrmDb_IoState::WRITING;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->m_ioBank = bank;

    // Write the transmit buffer to the proper bank
    auto buf = this->getDatabaseBuffer();
    this->writeOut_out(bank.e, buf);
}

void StaticPrmDb::LOAD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Samd21::StaticPrmDb_PrmBank& bank) {
    // Make sure there is nothing in progress
    switch (this->m_ioState) {
        case StaticPrmDb_IoState::IDLE:  // This is OK
            break;
        case StaticPrmDb_IoState::READING:
        case StaticPrmDb_IoState::WRITING:
            // A transaction is in progress. Reject this command
            return this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
    }

    this->m_ioState = StaticPrmDb_IoState::READING;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->m_ioBank = bank;

    // Read the bank into the transmit buffer
    auto buf = this->getDatabaseBuffer();
    this->readOut_out(bank.e, buf);
}

}  // namespace Samd21

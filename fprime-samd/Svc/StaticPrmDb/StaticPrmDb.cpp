// ======================================================================
// \title  StaticPrmDb.cpp
// \author tumbar
// \brief  cpp file for StaticPrmDb component implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticPrmDb/StaticPrmDb.hpp"
#include "Fw/Cmd/CmdResponseEnumAc.hpp"
#include "Fw/Prm/ParamValidEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/SuccessEnumAc.hpp"
#include "config/FwAssertArgTypeAliasAc.h"

#include <cstring>

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StaticPrmDb::StaticPrmDb(const char* const compName)
    : StaticPrmDbComponentBase(compName), m_ioState(StaticPrmDb_IoState::IDLE), m_opCode(0), m_cmdSeq(0) {}

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

            this->log_ACTIVITY_HI_LoadedDatabase(validRecords, invalidRecords);

            // Percolate the parameters to the rest of the system
            this->loadParameters();

            result = Fw::Success::SUCCESS;
        } else {
            result = Fw::Success::FAILURE;

            // Wipe the parameter database completely
            // This will also invalidate all the hashes (which is what we want)
            memset(txBuf.getData(), 0, txBuf.getSize());

            this->log_WARNING_HI_MismatchedParameterDatabaseHash(storedHash, generatedHash);
        }
    } else {
        result = Fw::Success::FAILURE;
        this->log_WARNING_HI_ReadFailed(status);
    }

    this->m_ioState = StaticPrmDb_IoState::IDLE;
    if (result == Fw::Success::SUCCESS) {
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

void StaticPrmDb::setPrm_handler(FwIndexType portNum, FwPrmIdType id, Fw::ParamBuffer& val) {
    if (this->m_ioState == StaticPrmDb_IoState::IDLE) {
        this->writeParam(id, val);
    } else {
        this->log_WARNING_HI_DroppingPrmSave(id, this->m_ioState);
    }
}

void StaticPrmDb::writeReturnIn_handler(FwIndexType portNum,
                                        Fw::Buffer& fwBuffer,
                                        const Drv::ByteStreamStatus& status) {
    // Make sure we are waiting for a reply
    FW_ASSERT(this->m_ioState == StaticPrmDb_IoState::WRITING, static_cast<FwAssertArgType>(this->m_ioState));

    // We should be getting back the same buffer we sent out
    auto txBuf = this->getDatabaseBuffer();
    FW_ASSERT(fwBuffer.getData() == txBuf.getData());

    this->m_ioState = StaticPrmDb_IoState::IDLE;
    if (status == Drv::ByteStreamStatus::OP_OK) {
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::OK);
    } else {
        // We failed to write the data down to the bank
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticPrmDb::SAVE_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
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

    // Write the transmit buffer to the proper bank
    auto buf = this->getDatabaseBuffer();
    Fw::ExternalSerializeBuffer ser(buf.getData(), buf.getSize());

    // Refresh the database hash
    auto serStatus = ser.serializeFrom(this->getDeploymentParameterHash());
    FW_ASSERT(serStatus, Fw::FW_SERIALIZE_OK, serStatus);

    this->writeOut_out(0, buf);
}

void StaticPrmDb::LOAD_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
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

    // Read the bank into the transmit buffer
    auto buf = this->getDatabaseBuffer();
    this->readOut_out(0, buf);
}

}  // namespace Samd21

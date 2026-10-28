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
            this->log_WARNING_HI_MismatchedParameterDatabaseHash(storedHash, generatedHash);
        }
    } else {
        result = Fw::Success::FAILURE;
        this->log_WARNING_HI_ReadFailed(status);
    }

    if (result == Fw::Success::SUCCESS) {
        this->m_ioState = StaticPrmDb_IoState::IDLE;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::OK);
    } else {
        this->m_ioState = StaticPrmDb_IoState::IDLE;
        this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    }
}

//! Shift a nibble (placed in the high bits) through 4 rounds of the MSB-first CRC-8 polynomial
static constexpr U8 crc8Nibble(U8 n) {
    U8 crc = static_cast<U8>(n << 4);
    for (U8 b = 0; b < 4; b++) {
        crc = (crc & 0x80) ? static_cast<U8>((crc << 1) ^ StaticPrmDb::CRC8_POLY) : static_cast<U8>(crc << 1);
    }
    return crc;
}

// Nibble-table CRC-8: 16-byte table, ~2 lookups per byte on M0+
static constexpr U8 CRC8_NIBBLE[16] = {
    crc8Nibble(0x0), crc8Nibble(0x1), crc8Nibble(0x2), crc8Nibble(0x3), crc8Nibble(0x4), crc8Nibble(0x5),
    crc8Nibble(0x6), crc8Nibble(0x7), crc8Nibble(0x8), crc8Nibble(0x9), crc8Nibble(0xA), crc8Nibble(0xB),
    crc8Nibble(0xC), crc8Nibble(0xD), crc8Nibble(0xE), crc8Nibble(0xF),
};

U8 StaticPrmDb::computeCrc(U8 seed, const U8* data, FwSizeType len) {
    U8 crc = seed ^ 0xFF;
    for (FwSizeType i = 0; i < len; i++) {
        crc ^= data[i];
        crc = static_cast<U8>((crc << 4) ^ CRC8_NIBBLE[crc >> 4]);
        crc = static_cast<U8>((crc << 4) ^ CRC8_NIBBLE[crc >> 4]);
    }
    return crc;
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

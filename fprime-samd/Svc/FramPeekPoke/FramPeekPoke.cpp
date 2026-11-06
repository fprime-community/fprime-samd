// ======================================================================
// \title  FramPeekPoke.cpp
// \author tumbar
// \brief  cpp file for FramPeekPoke component implementation class
// ======================================================================

#include "fprime-samd/Svc/FramPeekPoke/FramPeekPoke.hpp"
#include "Fw/Types/Assert.hpp"

namespace Samd21 {

// Definitions of the ODR-used class constants (C++14)
constexpr FwSizeType FramPeekPoke::MAX_WORD_SIZE;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FramPeekPoke ::FramPeekPoke(const char* const compName)
    : FramPeekPokeComponentBase(compName),
      m_busy(false),
      m_op(Fram::Op::READ),
      m_opCode(0),
      m_cmdSeq(0),
      m_offset(0),
      m_width(FramWordWidth::U8_WORD),
      m_word() {}

FramPeekPoke ::~FramPeekPoke() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void FramPeekPoke ::framReadComplete_handler(FwIndexType portNum,
                                             U32 offset,
                                             Fw::Buffer& buffer,
                                             const Fram::Status& status) {
    // Exactly one completion follows an accepted request (Fram.Request contract)
    FW_ASSERT(this->m_busy && (this->m_op == Fram::Op::READ), static_cast<FwAssertArgType>(this->m_op));
    if (status != Fram::Status::OK) {
        this->fail(status);
        return;
    }
    this->log_ACTIVITY_HI_Peeked(this->m_offset, this->m_width, this->wordValue());
    this->respond(Fw::CmdResponse::OK);
}

void FramPeekPoke ::framWriteComplete_handler(FwIndexType portNum,
                                              U32 offset,
                                              Fw::Buffer& buffer,
                                              const Fram::Status& status) {
    // Exactly one completion follows an accepted request (Fram.Request contract)
    FW_ASSERT(this->m_busy && (this->m_op == Fram::Op::WRITE), static_cast<FwAssertArgType>(this->m_op));
    if (status != Fram::Status::OK) {
        this->fail(status);
        return;
    }
    this->log_ACTIVITY_HI_Poked(this->m_offset, this->m_width, this->wordValue());
    this->respond(Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void FramPeekPoke ::PEEK_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 offset, const Samd21::FramWordWidth& width) {
    if (this->m_busy) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }
    this->start(Fram::Op::READ, opCode, cmdSeq, offset, width);
}

void FramPeekPoke ::POKE_cmdHandler(FwOpcodeType opCode,
                                    U32 cmdSeq,
                                    U32 offset,
                                    const Samd21::FramWordWidth& width,
                                    U32 value) {
    if (this->m_busy) {
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::BUSY);
        return;
    }
    const FwSizeType size = static_cast<FwSizeType>(width.e);
    if ((size < MAX_WORD_SIZE) && ((value >> (8 * size)) != 0)) {
        this->log_WARNING_LO_ValueTooWide(width, value);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::VALIDATION_ERROR);
        return;
    }
    // Big endian: the most significant byte of the word first
    for (FwSizeType i = 0; i < size; i++) {
        this->m_word[i] = static_cast<U8>(value >> (8 * (size - 1 - i)));
    }
    this->start(Fram::Op::WRITE, opCode, cmdSeq, offset, width);
}

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

void FramPeekPoke ::start(Fram::Op op, FwOpcodeType opCode, U32 cmdSeq, U32 offset, FramWordWidth width) {
    this->m_busy = true;
    this->m_op = op;
    this->m_opCode = opCode;
    this->m_cmdSeq = cmdSeq;
    this->m_offset = offset;
    this->m_width = width;

    // The driver may complete the request before the call returns
    Fw::Buffer word(this->m_word, static_cast<FwSizeType>(width.e));
    const Fram::Status status =
        (op == Fram::Op::READ) ? this->framRead_out(0, offset, word) : this->framWrite_out(0, offset, word);
    if (status != Fram::Status::ACCEPTED) {
        // Rejected (BUSY, OUT_OF_RANGE, WRITE_PROTECTED, SPI_ERROR): no completion follows
        this->fail(status);
    }
}

void FramPeekPoke ::fail(Fram::Status status) {
    this->log_WARNING_HI_AccessFailed(this->m_op, this->m_offset, this->m_width, status);
    this->respond(Fw::CmdResponse::EXECUTION_ERROR);
}

U32 FramPeekPoke ::wordValue() const {
    U32 value = 0;
    for (FwSizeType i = 0; i < static_cast<FwSizeType>(this->m_width.e); i++) {
        value = (value << 8) | this->m_word[i];
    }
    return value;
}

void FramPeekPoke ::respond(Fw::CmdResponse response) {
    // Idle first: the command response may lead straight to the next command
    this->m_busy = false;
    this->cmdResponse_out(this->m_opCode, this->m_cmdSeq, response);
}

}  // namespace Samd21

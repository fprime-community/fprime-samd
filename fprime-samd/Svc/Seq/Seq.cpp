// ======================================================================
// \title  Seq.cpp
// \author tumbar
// \brief  cpp file for Seq component implementation class
// ======================================================================

#include "fprime-samd/Svc/Seq/Seq.hpp"
#include "Fw/Cmd/CmdResponseEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "SeqConfig/SeqNamesEnumAc.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

Seq ::Seq(const char* const compName)
    : SeqComponentBase(compName),
      m_actions(nullptr),
      m_offset(),
      m_tableIdx(),
      m_state(),
      m_pendingTableIdx(SeqNames::NUM_CONSTANTS),
      m_runOnErrorTableIdx(SeqNames::NUM_CONSTANTS),
      m_sleepKind(SleepKind::NONE),
      m_waitTicks(0),
      m_waitTimeBase(),
      m_waitSeconds(0),
      m_waitUseconds(0) {}

Seq ::~Seq() {}

void Seq ::configure(const Action* actions) {
    FW_ASSERT(actions != nullptr);
    FW_ASSERT(this->m_actions == nullptr);

    this->m_actions = actions;
    this->m_tableIdx = Samd21::SeqNames();
    this->m_offset = 0;
    this->m_state = State::IDLE;
    this->m_pendingTableIdx = SeqNames::NUM_CONSTANTS;
    this->m_runOnErrorTableIdx = SeqNames::NUM_CONSTANTS;
    this->m_sleepKind = SleepKind::NONE;
}

void Seq::run(const Samd21::SeqNames& name) {
    this->m_pendingTableIdx = name;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

bool Seq ::activeIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_state == State::PENDING_NEXT) {
        this->next();

        // Work was done, we may need to re-cycle
        return true;
    } else if (this->m_state == State::IDLE && this->m_pendingTableIdx != SeqNames::NUM_CONSTANTS) {
        // There is a pending run and we are ready to dispatch it
        this->log_ACTIVITY_LO_StartingSequence(Samd21::SeqNames(this->m_pendingTableIdx));

        // Set up the sequence
        this->m_tableIdx = this->m_pendingTableIdx;
        this->m_pendingTableIdx = SeqNames::NUM_CONSTANTS;
        this->m_offset = 0;
        this->next();

        // Work was done, we may need to re-cycle
        return true;
    } else {
        return false;
    }
}

void Seq ::commandResponseIn_handler(FwIndexType portNum,
                                     FwOpcodeType opCode,
                                     U32 cmdSeq,
                                     const Fw::CmdResponse& response) {
    if (this->m_state != State::AWAITING_RESPONSE) {
        this->log_WARNING_HI_DroppingStrayResponse(opCode, cmdSeq, response);
        return;
    }

    if (response != Fw::CmdResponse::OK) {
        // A command in the action failed: abort the whole action.
        this->log_WARNING_LO_CommandFailed(opCode, cmdSeq, response);
        this->m_state = State::IDLE;
        this->m_sleepKind = SleepKind::NONE;

        // If a recovery table was armed, pend it so the next idle activeIn tick
        // picks it up. It is one-shot: clear it so a failing recovery sequence
        // does not re-trigger itself forever.
        if (this->m_runOnErrorTableIdx != SeqNames::NUM_CONSTANTS) {
            this->m_pendingTableIdx = this->m_runOnErrorTableIdx;
            this->m_runOnErrorTableIdx = SeqNames::NUM_CONSTANTS;
        }
    } else if (this->m_sleepKind != SleepKind::NONE) {
        // The command that just completed was a WAIT_* directive dispatched back
        // to this component. Enter the sleep it requested; schedIn services it.
        this->m_state = State::SLEEPING;
    } else if (!this->hasMoreRecordsInternal()) {
        // Last command completed successfully: the action is done.
        this->log_ACTIVITY_LO_FinishedSequence(this->m_tableIdx);
        this->m_state = State::IDLE;
    } else {
        // Wait for activeIn to pick up the next command
        this->m_state = State::PENDING_NEXT;
    }
}

void Seq ::schedIn_handler(FwIndexType portNum, U32 context) {
    if (this->m_state != State::SLEEPING) {
        return;
    }

    switch (this->m_sleepKind) {
        case SleepKind::TICKS:
            // WAIT_TICKS(n) blocks for n ticks. n == 0 resumes on this, the next
            // tick after the command; each remaining tick decrements the counter.
            if (this->m_waitTicks == 0) {
                this->m_sleepKind = SleepKind::NONE;
                this->next();
            } else {
                this->m_waitTicks--;
            }
            break;

        case SleepKind::UNTIL: {
            // WAIT_UNTIL wakes once the FSW clock reaches the requested base and
            // time. A differing base never matches, so the sleep persists.
            const Fw::Time now = this->getTime();
            const bool baseMatches = now.getTimeBase() == this->m_waitTimeBase;
            const bool timeReached =
                (now.getSeconds() > this->m_waitSeconds) ||
                (now.getSeconds() == this->m_waitSeconds && now.getUSeconds() >= this->m_waitUseconds);
            if (baseMatches && timeReached) {
                this->m_sleepKind = SleepKind::NONE;
                this->next();
            }
            break;
        }

        case SleepKind::NONE:
        default:
            // SLEEPING with no wait kind is unreachable: the state is only
            // entered after a WAIT_* command sets the kind.
            FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_sleepKind));
            break;
    }
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void Seq ::RUN_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Samd21::SeqNames& table) {
    FW_ASSERT(this->m_actions != nullptr);

    this->m_pendingTableIdx = table;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Seq ::RUN_ON_ERROR_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const Samd21::SeqNames& table) {
    FW_ASSERT(this->m_actions != nullptr);

    // Arm the recovery table and clear any pending normal RUN.
    this->m_runOnErrorTableIdx = table;
    this->m_pendingTableIdx = SeqNames::NUM_CONSTANTS;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Seq ::CANCEL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Stop whatever is executing or pending and reset the program counter to the
    // start of the sequence. Nothing is dispatched. If a command was already in
    // flight (AWAITING_RESPONSE), its response will arrive after we return to
    // IDLE and be dropped as a stray response.

    // Note where we were before resetting so the event reflects the cancelled
    // point in the sequence.
    this->log_ACTIVITY_HI_Cancelled(this->m_tableIdx, this->m_offset);

    this->m_state = State::IDLE;
    this->m_offset = 0;
    this->m_pendingTableIdx = SeqNames::NUM_CONSTANTS;
    this->m_runOnErrorTableIdx = SeqNames::NUM_CONSTANTS;
    this->m_sleepKind = SleepKind::NONE;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Seq ::WAIT_TICKS_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 n) {
    // Record the wait and acknowledge. This handler runs re-entrantly while the
    // dispatched WAIT_TICKS command is AWAITING_RESPONSE; the OK response below
    // drives commandResponseIn, which observes m_sleepKind and enters SLEEPING.
    this->m_sleepKind = SleepKind::TICKS;
    this->m_waitTicks = n;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void Seq ::WAIT_UNTIL_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, const TimeBase& timeBase, U32 seconds, U32 useconds) {
    // Record the absolute wakeup and acknowledge. See WAIT_TICKS_cmdHandler for
    // the re-entrant handoff to commandResponseIn / SLEEPING.
    this->m_sleepKind = SleepKind::UNTIL;
    this->m_waitTimeBase = timeBase;
    this->m_waitSeconds = seconds;
    this->m_waitUseconds = useconds;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

bool Seq ::hasMoreRecordsInternal() const {
    return this->m_offset < this->m_actions[this->m_tableIdx].m_len;
}

U16 Seq ::deserializeRecord(Fw::ExternalSerializeBuffer& cmd) {
    Fw::ExternalSerializeBuffer buffer(const_cast<U8*>(this->m_actions[this->m_tableIdx].m_data),
                                       this->m_actions[this->m_tableIdx].m_len);

    buffer.moveSerToOffset(this->m_actions[this->m_tableIdx].m_len);
    buffer.moveDeserToOffset(this->m_offset);

    // Command size: the number of bytes spanning the opcode plus its arguments.
    U32 cmdSize = 0;
    Fw::SerializeStatus status = buffer.deserializeTo(cmdSize);
    if (status != Fw::FW_SERIALIZE_OK || cmdSize < sizeof(FwOpcodeType) || cmdSize > buffer.getDeserializeSizeLeft()) {
        this->log_WARNING_HI_InvalidSequence(cmdSize, this->m_offset);
        return 0;
    }

    cmd.setExtBuffer(const_cast<U8*>(buffer.getBuffAddrLeft()), cmdSize);
    cmd.moveSerToOffset(cmdSize);

    // Advance the parent buffer past the command payload so the consumed-byte
    // count below spans the whole record (header + command), not just the header.
    status = buffer.deserializeSkip(cmdSize);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Bytes consumed by this record (delta from the record start offset).
    return static_cast<U16>((this->m_actions[this->m_tableIdx].m_len - buffer.getDeserializeSizeLeft()) -
                            this->m_offset);
}

void Seq ::dispatchRecord(const Fw::ExternalSerializeBuffer& cmdBuf, U32 cmdSeq) {
    // Copy the args into a local "owned" spot
    Fw::ComBuffer cmd(cmdBuf.getBuffAddr(), cmdBuf.getSize());
    this->commandOut_out(0, cmd, cmdSeq);
}

void Seq ::next() {
    // The program counter can already be at the end of the buffer when the last
    // command in a sequence is a WAIT_* directive: the wait advanced the offset
    // past the final command, and schedIn calls next() once the wait expires.
    // Finish cleanly rather than deserializing a zero-length record off the end.
    if (!this->hasMoreRecordsInternal()) {
        this->log_ACTIVITY_LO_FinishedSequence(this->m_tableIdx);
        this->m_state = State::IDLE;
        return;
    }

    Fw::ExternalSerializeBuffer cmd;
    U16 bytesRead = this->deserializeRecord(cmd);
    if (bytesRead == 0) {
        this->m_state = State::IDLE;
        return;
    }

    // We are actually ready to dispatch the record, increment
    // program counter and wait for reply
    U32 cmdSeq = this->m_offset;
    this->m_offset += bytesRead;
    this->m_state = State::AWAITING_RESPONSE;
    this->dispatchRecord(cmd, cmdSeq);
}

}  // namespace Samd21

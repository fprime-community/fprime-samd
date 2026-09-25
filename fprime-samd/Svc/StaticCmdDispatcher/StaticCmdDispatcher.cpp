// ======================================================================
// \title  StaticCmdDispatcher.cpp
// \author tumbar
// \brief  cpp file for StaticCmdDispatcher component implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticCmdDispatcher/StaticCmdDispatcher.hpp"
#include "Fw/Cmd/CmdPacket.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/SuccessEnumAc.hpp"

#include <limits>

namespace Samd21 {

// Check the CMD_DISPATCHER_SEQUENCER_TABLE_SIZE constant for overflow
static_assert(CMD_DISPATCHER_SEQUENCER_TABLE_SIZE <= std::numeric_limits<U32>::max(),
              "Sequencer table limited to range of U32");

// Indicates that an entry in the sequence tracker is unused
constexpr FwOpcodeType OPCODE_UNUSED = std::numeric_limits<FwOpcodeType>::max();

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StaticCmdDispatcher ::StaticCmdDispatcher(const char* const compName)
    : StaticCmdDispatcherComponentBase(compName), m_seq(0), m_eventDisabled() {
    for (auto i = 0; i < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; i++) {
        this->m_sequenceTracker[i].opcode = OPCODE_UNUSED;
    }
}

StaticCmdDispatcher ::~StaticCmdDispatcher() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void StaticCmdDispatcher ::compCmdReg_handler(FwIndexType portNum, FwOpcodeType opCode) {
    // The opcode -> port mapping is already known statically from the topology, so
    // there is nothing to register at runtime. Just sanity check that the autocoded
    // table agrees with the connection this registration came in on.
    FwIndexType port = -1;
    auto success = this->lookupDispatchPort(opCode, port);
    FW_ASSERT(success == Fw::Success::SUCCESS && (port == portNum), static_cast<FwAssertArgType>(opCode));
}

void StaticCmdDispatcher ::compCmdStat_handler(FwIndexType portNum,
                                               FwOpcodeType opCode,
                                               U32 cmdSeq,
                                               const Fw::CmdResponse& response) {
    // This opcode is reserved for internal use
    FW_ASSERT(opCode != OPCODE_UNUSED, portNum);

    // Search for the command source
    FwIndexType portToCall = -1;
    U32 context = 0;
    for (auto pending = 0; pending < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; pending++) {
        auto entry = &this->m_sequenceTracker[pending];
        if ((entry->opcode != OPCODE_UNUSED) && (entry->seq == cmdSeq)) {
            portToCall = entry->callerPort;
            context = entry->context;
            FW_ASSERT(opCode == entry->opcode);
            FW_ASSERT(portToCall < this->getNum_seqCmdStatus_OutputPorts());
            // Free up the sequence tracker entry
            entry->opcode = OPCODE_UNUSED;
            break;
        }
    }

    FW_ASSERT(portToCall < NUM_SEQCMDSTATUS_OUTPUT_PORTS);

    if (portToCall != -1) {
        // Check the command response and log success/failure
        if (response.e == Fw::CmdResponse::OK) {
            if (!m_eventDisabled[portToCall]) {
                this->log_COMMAND_OpCodeCompleted(opCode);
            }
        } else {
            FW_ASSERT(response.e != Fw::CmdResponse::OK);
            this->log_COMMAND_OpCodeError(opCode, response);
        }

        if (this->isConnected_seqCmdStatus_OutputPort(portToCall)) {
            // NOTE: seqCmdStatus port forwards three arguments (opCode, cmdSeq, response) but the
            // cmdSeq value has no meaning for the calling sequencer; instead, the context value is
            // forwarded to allow the caller to utilize it if needed.
            this->seqCmdStatus_out(portToCall, opCode, context, response);
        }
    } else {
        this->log_WARNING_LO_UnexpectedCommandResponse(opCode, cmdSeq, response);
    }
}

void StaticCmdDispatcher ::seqCmd_helper(FwIndexType portNum,
                                         FwOpcodeType opcode,
                                         U32 context,
                                         Fw::CmdArgBuffer& args) {
    // Look up the output port for this opcode. Ignore OPCODE_UNUSED, reserved for internal use
    FwIndexType port = -1;
    bool found = (opcode != OPCODE_UNUSED) && this->lookupDispatchPort(opcode, port) == Fw::Success::SUCCESS;

    if (found && this->isConnected_compCmdSend_OutputPort(port)) {
        // Register the command in the command tracker only if the response port is connected
        if (this->isConnected_seqCmdStatus_OutputPort(portNum)) {
            bool pendingFound = false;
            for (U32 pending = 0; pending < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; pending++) {
                SequenceTracker* trackerEntry = &this->m_sequenceTracker[pending];
                if (trackerEntry->opcode == OPCODE_UNUSED) {
                    pendingFound = true;
                    trackerEntry->opcode = opcode;
                    trackerEntry->callerPort = portNum;
                    trackerEntry->seq = this->m_seq;
                    trackerEntry->context = context;
                    break;
                }
            }
            // If no slot was found to track the command, quit
            if (!pendingFound) {
                this->log_WARNING_HI_TooManyCommands(opcode);
                if (this->isConnected_seqCmdStatus_OutputPort(portNum)) {
                    this->seqCmdStatus_out(portNum, opcode, context, Fw::CmdResponse::EXECUTION_ERROR);
                }
                return;
            }
        }

        // Pass arguments to the argument buffer and log the dispatched command
        this->compCmdSend_out(port, opcode, this->m_seq, args);
        if (!this->m_eventDisabled[portNum]) {
            this->log_COMMAND_OpCodeDispatched(opcode, port);
        }
    } else {
        // Opcode could not be found in the dispatch table, fail the command
        this->log_WARNING_HI_InvalidCommand(opcode);
        if (this->isConnected_seqCmdStatus_OutputPort(portNum)) {
            this->seqCmdStatus_out(portNum, opcode, context, Fw::CmdResponse::INVALID_OPCODE);
        }
    }

    // Increment sequence number
    this->m_seq++;
}

void StaticCmdDispatcher ::seqCmdBuff_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    // Deserialize the command packet
    Fw::CmdPacket cmdPkt;
    Fw::SerializeStatus stat = cmdPkt.deserializeFrom(data);
    if (stat != Fw::FW_SERIALIZE_OK) {
        Fw::DeserialStatus serErr = static_cast<Fw::DeserialStatus::t>(stat);
        this->log_WARNING_HI_MalformedCommand(serErr);
        if (this->isConnected_seqCmdStatus_OutputPort(portNum)) {
            this->seqCmdStatus_out(portNum, cmdPkt.getOpCode(), context, Fw::CmdResponse::VALIDATION_ERROR);
        }
        return;
    }
    this->seqCmd_helper(portNum, cmdPkt.getOpCode(), context, cmdPkt.getArgBuffer());
}

void StaticCmdDispatcher ::seqCmdIn_handler(FwIndexType portNum,
                                            FwOpcodeType opCode,
                                            U32 cmdSeq,
                                            Fw::CmdArgBuffer& args) {
    this->seqCmd_helper(portNum, opCode, cmdSeq, args);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticCmdDispatcher ::CMD_NO_OP_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    this->log_ACTIVITY_HI_NoOpReceived();
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void StaticCmdDispatcher ::CMD_CLEAR_TRACKING_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    // Clear the sequence tracking table
    for (FwOpcodeType entry = 0; entry < CMD_DISPATCHER_SEQUENCER_TABLE_SIZE; entry++) {
        this->m_sequenceTracker[entry].opcode = OPCODE_UNUSED;
    }
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void StaticCmdDispatcher ::SET_EVENT_EMISSION_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U8 portIdx, bool enabled) {
    if (portIdx >= NUM_SEQCMDSTATUS_OUTPUT_PORTS) {
        this->log_WARNING_LO_PortIndexOutOfRange(portIdx);
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::EXECUTION_ERROR);
    } else {
        this->m_eventDisabled[portIdx] = !enabled;
        this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
    }
}

}  // namespace Samd21

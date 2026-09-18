// ======================================================================
// \title  StaticCmdDispatcher.hpp
// \author tumbar
// \brief  hpp file for StaticCmdDispatcher component implementation class
// ======================================================================

#ifndef Samd21_StaticCmdDispatcher_HPP
#define Samd21_StaticCmdDispatcher_HPP

#include <config/CommandDispatcherImplCfg.hpp>
#include "Fw/Types/SuccessEnumAc.hpp"
#include "fprime-samd/Svc/StaticCmdDispatcher/StaticCmdDispatcherComponentAc.hpp"

namespace Samd21 {

class StaticCmdDispatcher final : public StaticCmdDispatcherComponentBase {
  private:
    // ----------------------------------------------------------------------
    // Type definitions
    // ----------------------------------------------------------------------

    struct SequenceTracker {
        // unused entries have the opcode set to OPCODE_UNUSED (see .cpp for definition)
        FwOpcodeType opcode;     //!< opcode being tracked
        FwIndexType callerPort;  //!< port command source port
        U32 seq;                 //!< command sequence number
        U32 context;             //!< context passed by user
    };

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StaticCmdDispatcher object
    StaticCmdDispatcher(const char* const compName  //!< The component name
    );

    //! Destroy StaticCmdDispatcher object
    ~StaticCmdDispatcher();

  private:
    // ----------------------------------------------------------------------
    // Helper functions for input ports
    // ----------------------------------------------------------------------

    void seqCmd_helper(FwIndexType portNum, FwOpcodeType opCode, U32 cmdSeq, Fw::CmdArgBuffer& args);

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for compCmdReg
    //!
    //! Command registration input ports. Size must match the dispatch output ports.
    void compCmdReg_handler(FwIndexType portNum,  //!< The port number
                            FwOpcodeType opCode   //!< Command Op Code
                            ) override;

    //! Handler implementation for compCmdStat
    //!
    //! Input command status ports
    void compCmdStat_handler(FwIndexType portNum,             //!< The port number
                             FwOpcodeType opCode,             //!< Command Op Code
                             U32 cmdSeq,                      //!< Command Sequence
                             const Fw::CmdResponse& response  //!< The command response argument
                             ) override;

    //! Handler implementation for seqCmdBuff
    //!
    //! Command buffer input port for sequencers or other sources of command buffers
    void seqCmdBuff_handler(FwIndexType portNum,  //!< The port number
                            Fw::ComBuffer& data,  //!< Buffer containing packet data
                            U32 context           //!< Call context value; meaning chosen by user
                            ) override;

    //! Handler implementation for seqCmdIn
    //!
    //! Command input port for sequencers or other sources of commands
    void seqCmdIn_handler(FwIndexType portNum,    //!< The port number
                          FwOpcodeType opCode,    //!< Command Op Code
                          U32 cmdSeq,             //!< Command Sequence
                          Fw::CmdArgBuffer& args  //!< Buffer containing arguments
                          ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command CMD_NO_OP
    //!
    //! No-op command
    void CMD_NO_OP_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                              U32 cmdSeq            //!< The command sequence number
                              ) override;

    //! Handler implementation for command CMD_CLEAR_TRACKING
    //!
    //! Clear command tracking info to recover from components that are not returning status
    void CMD_CLEAR_TRACKING_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq            //!< The command sequence number
                                       ) override;

    //! Handler implementation for command SET_EVENT_EMISSION
    //!
    //! Enable or dispatch OpCodeDispatched/OpCodeCompleted events for commands that come in on a certain port index
    void SET_EVENT_EMISSION_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                       U32 cmdSeq,           //!< The command sequence number
                                       U8 portIdx,
                                       bool enabled) override;

  private:
    //! Look up the output port connected to the component that handles a given opcode
    //! This function is autocoded at the topology level
    Fw::Success lookupDispatchPort(FwOpcodeType opcode, FwIndexType& port);

    //!< Current command sequence number
    U32 m_seq;

    //! Tracks commands that are being executed but not yet complete
    SequenceTracker m_sequenceTracker[CMD_DISPATCHER_SEQUENCER_TABLE_SIZE];

    //! Tracks whether or not to emit OpCodeDispatched/OpCodeCompleted events for each incoming port index
    bool m_eventDisabled[NUM_SEQCMDSTATUS_OUTPUT_PORTS];
};

}  // namespace Samd21

#endif

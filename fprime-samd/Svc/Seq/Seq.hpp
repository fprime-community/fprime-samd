// ======================================================================
// \title  Seq.hpp
// \author tumbar
// \brief  hpp file for Seq component implementation class
// ======================================================================

#ifndef Samd21_Seq_HPP
#define Samd21_Seq_HPP

#include "Fw/Types/Serializable.hpp"
#include "SeqConfig/SeqNamesEnumAc.hpp"
#include "fprime-samd/Svc/Seq/SeqComponentAc.hpp"

namespace Samd21 {

class Seq final : public SeqComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Seq object
    Seq(const char* const compName  //!< The component name
    );

    //! Destroy Seq object
    ~Seq();

    //! A single hard-coded action: a pointer to its binary blob and the blob length
    struct Action {
        Action() : m_data(nullptr), m_len(0) {}
        Action(const U8* data, U32 len) : m_data(data), m_len(len) {}

        const U8* m_data;  //!< Pointer to the action binary (typically in flash)
        U32 m_len;         //!< Length of the action binary in bytes
    };

    //! Configure the action list.
    //! \a actions must point at an array of exactly SeqNames::NUM_CONSTANTS
    //! entries, indexed by the SeqNames enum value. The array and the
    //! binaries it references must remain valid for the lifetime of the component.
    void configure(const Action* actions);

    //! Pend an action to run when the MCU boots into the main loop
    void run(const Samd21::SeqNames& name);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for activeIn
    //!
    //! Active in for dispatching the "next" action in the table or reseting
    bool activeIn_handler(FwIndexType portNum,  //!< The port number
                          U32 context           //!< The call order
                          ) override;

    //! Handler implementation for commandResponseIn
    //!
    //! Port to receive status of the currently executing action
    void commandResponseIn_handler(FwIndexType portNum,             //!< The port number
                                   FwOpcodeType opCode,             //!< Command Op Code
                                   U32 cmdSeq,                      //!< Command Sequence
                                   const Fw::CmdResponse& response  //!< The command response argument
                                   ) override;

    //! Handler implementation for schedIn
    //!
    //! Check timers for wait/wait_until timeouts
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command RUN
    //!
    //! Run an action table after the current execution finishes
    //! When this component enters idle, it will pick up this pending run
    void RUN_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                        U32 cmdSeq,           //!< The command sequence number
                        const Samd21::SeqNames& table) override;

    //! Handler implementation for command RUN_ON_ERROR
    //!
    //! Set the table to run if we exit with an error (i.e. command failure).
    //! The pending `RUN` is cleared.
    void RUN_ON_ERROR_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                 U32 cmdSeq,           //!< The command sequence number
                                 const Samd21::SeqNames& table) override;

    //! Handler implementation for command CANCEL
    //!
    //! Stop any executing or pending action and reset the program counter to the
    //! start of the sequence without dispatching anything. A command response for
    //! an already-dispatched action may still arrive afterwards; it is dropped.
    void CANCEL_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                           U32 cmdSeq            //!< The command sequence number
                           ) override;

    //! Handler implementation for command WAIT_TICKS
    //!
    //! Block for a number of rate groups ticks before continuing
    //! If n == 0, the sequence will resume on the NEXT rate group tick
    void WAIT_TICKS_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                               U32 cmdSeq,           //!< The command sequence number
                               U32 n) override;

    //! Handler implementation for command WAIT_UNTIL
    //!
    //! Block the sequence until the given timeBase matches and the time >= seconds.useconds
    void WAIT_UNTIL_cmdHandler(FwOpcodeType opCode,       //!< The opcode
                               U32 cmdSeq,                //!< The command sequence number
                               const TimeBase& timeBase,  //!< Wait for the FSW to reach this timebase
                               U32 seconds,               //!< Wait for FSW time to reach this time in seconds
                               U32 useconds               //!< Microsecond offset to wait for FSW
                               ) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Whether the current action has more command bytes left to deserialize
    bool hasMoreRecordsInternal() const;

    //! Deserialize the next command from the current action into \a cmd.
    //! \return the number of bytes consumed, or 0 on a malformed command.
    U16 deserializeRecord(Fw::ExternalSerializeBuffer& cmd);

    //! Dispatch the command held in \a cmdBuf out the commandOut port.
    void dispatchRecord(const Fw::ExternalSerializeBuffer& cmdBuf, U32 cmdSeq);

    //! Advance to and dispatch the next command. If the end of the action is
    //! reached, finish successfully. On a malformed command, finish with failure.
    void next();

  private:
    // ----------------------------------------------------------------------
    // Member state
    // ----------------------------------------------------------------------

    static_assert(SeqNames::NUM_CONSTANTS < 255, "Samd21::Seq does not support >255 tables");

    enum class State : U8 {
        IDLE,
        AWAITING_RESPONSE,
        PENDING_NEXT,
        SLEEPING,
    };

    //! What the component is waiting on while SLEEPING. Set by the WAIT_TICKS /
    //! WAIT_UNTIL command handlers (which run re-entrantly while the just-issued
    //! wait command is AWAITING_RESPONSE) and consumed by commandResponseIn to
    //! move the component into SLEEPING.
    enum class SleepKind : U8 {
        NONE,   //!< Not waiting; the completed command advances to the next one
        TICKS,  //!< Waiting for a number of rate-group ticks (WAIT_TICKS)
        UNTIL,  //!< Waiting for the FSW clock to reach an absolute time (WAIT_UNTIL)
    };

    const Action* m_actions;  //!< The configured action list (NUM_CONSTANTS long)
    U16 m_offset;             //!< Current offset in bytes from the active action table
    U8 m_tableIdx;            //!< Index of the active table
    volatile State m_state;   //!< Whether an action is currently executing
    U8 m_pendingTableIdx;  //!< NUM_CONSTANTS indicates nothing to run. Otherwise once this component enters idle, start
                           //!< this sequence
    U8 m_runOnErrorTableIdx;  //!< NUM_CONSTANTS indicates none. Otherwise the table to pend when a command fails

    SleepKind m_sleepKind;    //!< The kind of wait requested by the last WAIT_* command
    U32 m_waitTicks;          //!< Rate-group ticks remaining before a WAIT_TICKS sleep resumes
    TimeBase m_waitTimeBase;  //!< Time base a WAIT_UNTIL sleep waits on
    U32 m_waitSeconds;        //!< Seconds a WAIT_UNTIL sleep waits for
    U32 m_waitUseconds;       //!< Microsecond offset a WAIT_UNTIL sleep waits for
};

}  // namespace Samd21

#endif

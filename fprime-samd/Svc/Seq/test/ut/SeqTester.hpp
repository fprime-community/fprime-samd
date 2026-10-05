// ======================================================================
// \title  SeqTester.hpp
// \author tumbar
// \brief  hpp file for Seq component test harness implementation class
// ======================================================================

#ifndef Samd21_SeqTester_HPP
#define Samd21_SeqTester_HPP

#include "fprime-samd/Svc/Seq/Seq.hpp"
#include "fprime-samd/Svc/Seq/SeqGTestBase.hpp"

namespace Samd21 {

class SeqTester final : public SeqGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs.
    // The component emits several Debug activity events per record, so the
    // history must comfortably exceed the number of records in a sequence.
    static const FwSizeType MAX_HISTORY_SIZE = 1000;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // The 4-byte opcodes exercised by the GENERIC sequence
    static const FwOpcodeType OPCODE_A = 0x20;  //!< dispatched by commands 0..3
    static const FwOpcodeType OPCODE_B = 0xaa;  //!< dispatched by the final command

    // On-wire size of a WAIT_* command in an action body: cmdSize field (4) +
    // opcode (4) + args. WAIT_TICKS carries one U32 (n); WAIT_UNTIL carries a
    // TimeBase (2) + seconds (4) + useconds (4) == 10 arg bytes.
    static const U32 WAIT_TICKS_STRIDE = 4 + 4 + 4;   //!< 12 bytes
    static const U32 WAIT_UNTIL_STRIDE = 4 + 4 + 10;  //!< 18 bytes

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object SeqTester
    SeqTester();

    //! Destroy object SeqTester
    ~SeqTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Run a multi-command sequence end to end and verify every command
    //! dispatches in order with the right args and byte offsets.
    void testGenericSequence();

    //! A RUN command is accepted and only starts once the component is idle and
    //! activeIn ticks.
    void testRunCommandPendsUntilActive();

    //! An immediate sequence dispatches one command per activeIn tick, gated by
    //! command responses.
    void testImmediateDispatchOrder();

    //! A failing command response aborts the remainder of the sequence.
    void testCommandFailureAborts();

    //! A command response that arrives while not awaiting one is dropped and
    //! logged.
    void testStrayResponseDropped();

    //! A second RUN issued while a sequence is executing is remembered and runs
    //! after the first completes.
    void testPendingRunAfterCompletion();

    //! A command whose cmdSize is smaller than a bare opcode is rejected and
    //! emits InvalidSequence.
    void testInvalidCmdSizeFails();

    //! A command whose cmdSize runs past the end of the action buffer is
    //! rejected and emits InvalidSequence.
    void testCmdSizeOverrunsBufferFails();

    //! An empty action (zero-length) is a valid empty sequence: it finishes
    //! immediately with no dispatch or warning.
    void testEmptyAction();

    //! A command truncated within the cmdSize field fails to deserialize and
    //! emits InvalidSequence.
    void testTruncatedCmdSizeFails();

    // -- WAIT_TICKS -------------------------------------------------------

    //! WAIT_TICKS(0) resumes the sequence on the very next rate-group tick.
    void testWaitTicksZeroResumesNextTick();

    //! WAIT_TICKS(n) blocks for exactly n rate-group ticks, then resumes.
    void testWaitTicksBlocksNTicks();

    //! A WAIT_TICKS that is the last command in the sequence finishes cleanly on
    //! wake without a spurious InvalidSequence off the end of the buffer.
    void testWaitTicksAtEndOfSequence();

    // -- WAIT_UNTIL -------------------------------------------------------

    //! WAIT_UNTIL sleeps until the FSW clock reaches the requested time.
    void testWaitUntilSleepsUntilTime();

    //! WAIT_UNTIL wakes when the FSW clock exactly equals the requested time.
    void testWaitUntilWakesOnExactTime();

    //! WAIT_UNTIL whose requested time is already in the past resumes on the
    //! next schedIn tick.
    void testWaitUntilPastResumesImmediately();

    //! WAIT_UNTIL on a time base the FSW clock never reports never wakes.
    void testWaitUntilMismatchedBaseNeverWakes();

    // -- RUN_ON_ERROR -----------------------------------------------------

    //! RUN_ON_ERROR arms a recovery table that starts after a command failure.
    void testRunOnErrorStartsRecovery();

    //! RUN_ON_ERROR does not trigger when the sequence completes successfully.
    void testRunOnErrorNotTriggeredOnSuccess();

    //! Issuing RUN_ON_ERROR clears any pending normal RUN.
    void testRunOnErrorClearsPendingRun();

    //! The armed recovery is one-shot: a second failure does not re-trigger it.
    void testRunOnErrorIsOneShot();

    // -- CANCEL -----------------------------------------------------------

    //! CANCEL while a WAIT is sleeping stops the sleep: schedIn no longer
    //! dispatches, even once the original wake time is reached.
    void testCancelWhileSleeping();

    //! CANCEL while awaiting a command response stops execution; the late
    //! response is then dropped as stray and nothing further dispatches.
    void testCancelWhileAwaitingResponse();

    //! CANCEL clears a pending RUN so it does not start on the next activeIn.
    void testCancelClearsPendingRun();

    //! CANCEL resets the program counter: a RUN after CANCEL re-executes the
    //! sequence from the first command.
    void testCancelResetsToStart();

    //! CANCEL while idle with nothing pending is a harmless no-op.
    void testCancelWhileIdleNoOp();

    //! CANCEL clears an armed RUN_ON_ERROR recovery table.
    void testCancelClearsRunOnError();

  private:
    // ----------------------------------------------------------------------
    // Handlers (override to feed the component a controllable time)
    // ----------------------------------------------------------------------

    // Time is provided through the auto-generated setTestTime() mechanism.

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Configure the component with a single-entry action list pointing at the
    //! given binary blob (indexed under SeqNames::GENERIC).
    void configureWith(const U8* data, U32 len);

    //! Set the component's notion of "now" (seconds only).
    void setNow(U32 seconds);

    //! Set the component's notion of "now" with microsecond precision.
    void setNowUs(U32 seconds, U32 useconds);

    //! Advance "now" by \a deltaSeconds and push it to the component.
    void advance(U32 deltaSeconds);

    //! Issue the RUN command for the GENERIC table.
    void run();

    //! One activeIn tick (dispatch next / pick up pending run).
    void tick();

    //! One schedIn tick (service any pending sleep).
    void sched();

    //! Reply to the most recently dispatched command with \a response.
    void respondLast(const Fw::CmdResponse& response);

    //! Simulate the command-dispatcher round-trip for a just-dispatched
    //! WAIT_TICKS command: run the WAIT_TICKS handler with \a n, then route its
    //! OK response back to commandResponseIn so the component enters SLEEPING.
    void completeWaitTicks(U32 n);

    //! As completeWaitTicks(), but for a WAIT_UNTIL command with the given
    //! wake-up time base and time.
    void completeWaitUntil(const TimeBase& base, U32 seconds, U32 useconds);

    //! Offset (Com port context) of the most recently dispatched command.
    U32 lastDispatchOffset();

    //! Assert that exactly \a n commands have been dispatched so far.
    void assertDispatchCount(U32 n);

    //! Assert that dispatch \a index carried the given opcode, total size (bytes),
    //! and cmdSeq/context (the command's byte offset in the action blob).
    void assertDispatch(U32 index, FwOpcodeType opcode, U32 totalSize, U32 cmdSeq);

  private:
    // ----------------------------------------------------------------------
    // A tiny builder for action binaries
    // ----------------------------------------------------------------------

    //! Assembles Seq command-buffer lists in the big-endian on-flash
    //! format: a flat list of (cmdSize:U32, cmd:U8[cmdSize]) pairs.
    struct ActionBuilder {
        ActionBuilder() : m_len(0) {}

        //! Append one dispatchable command. The command payload is
        //! (opcode || args), so cmdSize = 4 + argLen.
        void addCommand(FwOpcodeType opcode, const U8* args, U32 argLen);

        //! Append a WAIT_TICKS command buffer (placeholder arg; the test drives
        //! the tick count via completeWaitTicks()).
        void addWaitTicks();

        //! Append a WAIT_UNTIL command buffer (placeholder args; the test drives
        //! the wake time via completeWaitUntil()).
        void addWaitUntil();

        //! Append a single raw byte (used to inject malformed data).
        void addByte(U8 b);

        //! Append a big-endian U32 (used to inject malformed cmdSize headers).
        void addU32(U32 v);

        const U8* data() const { return m_buf; }
        U32 len() const { return m_len; }

      private:
        void putU8(U8 v);
        void putU32(U32 v);

        static const U32 CAP = 256;
        U8 m_buf[CAP];
        U32 m_len;
    };

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    Seq component;

    //! Backing storage for the action list handed to configure()
    Seq::Sequence m_actions[SeqNames::NUM_CONSTANTS];

    //! Current test time in seconds (fed to the component via setTestTime).
    U32 m_nowSeconds;
};

}  // namespace Samd21

#endif

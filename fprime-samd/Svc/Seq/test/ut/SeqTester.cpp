// ======================================================================
// \title  SeqTester.cpp
// \author tumbar
// \brief  cpp file for Seq component test harness implementation class
// ======================================================================

#include "SeqTester.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Pick/Pick.hpp"
#include "config/TimeBaseEnumAc.hpp"

namespace Samd21 {

// Time base fed to the component under test. This mirrors the flight time
// provider (Samd21Time emits TB_PROC_TIME), which is deliberately NOT the
// default TB_NONE, so WAIT_UNTIL base matching is exercised against a realistic
// base rather than the serialization default.
static const TimeBase TEST_TIME_BASE = TimeBase::TB_PROC_TIME;

// A GENERIC action body in the current command-buffer-list format: a flat list
// of (cmdSize:U32, cmd:U8[cmdSize]) pairs with no descriptor or time tag. It
// decodes to five commands:
//
//   idx  offset  opcode  argLen  cmdSize  stride
//   0    0       0x20    1       5        9
//   1    9       0x20    1       5        9
//   2    18      0x20    1       5        9
//   3    27      0x20    1       5        9
//   4    36      0xaa    5       9        13
//
// (cmdSize = 4-byte opcode + args; stride = 4-byte cmdSize field + cmdSize.)
// Byte-for-byte so the test does not depend on a generated build artifact.
static const U8 fprime_seq_GENERIC[] = {
    0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x20, 0x01,                          // idx 0
    0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x20, 0x01,                          // idx 1
    0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x20, 0x01,                          // idx 2
    0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x20, 0x01,                          // idx 3
    0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0xaa, 0x40, 0x00, 0x00, 0x00, 0x00,  // idx 4
};
static const U32 fprime_seq_GENERIC_len = 49u;

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SeqTester ::SeqTester() : SeqGTestBase("SeqTester", SeqTester::MAX_HISTORY_SIZE), component("Seq"), m_nowSeconds(0) {
    this->initComponents();
    this->connectPorts();
}

SeqTester ::~SeqTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SeqTester ::testGenericSequence() {
    REQUIREMENT("MFPM-ACT-001");
    REQUIREMENT("MFPM-ACT-002");
    this->configureWith(fprime_seq_GENERIC, fprime_seq_GENERIC_len);
    this->setNow(1000);
    this->run();

    // The five commands, in order: opcode, total command size, and the byte
    // offset that Seq passes as the command sequence/context.
    struct Expected {
        FwOpcodeType opcode;
        U32 size;
        U32 offset;
    };
    const Expected commands[] = {
        {OPCODE_A, 5, 0}, {OPCODE_A, 5, 9}, {OPCODE_A, 5, 18}, {OPCODE_A, 5, 27}, {OPCODE_B, 9, 36},
    };
    const U32 numCommands = FW_NUM_ARRAY_ELEMENTS(commands);

    for (U32 i = 0; i < numCommands; i++) {
        // Ask the component to advance to the next command.
        this->tick();

        this->assertDispatchCount(i + 1);
        this->assertDispatch(i, commands[i].opcode, commands[i].size, commands[i].offset);

        // Acknowledge success so the component arms the following command.
        this->respondLast(Fw::CmdResponse::OK);
    }

    // After the final command's OK, the sequence is complete. Further ticks are
    // no-ops and dispatch nothing more.
    this->tick();
    this->sched();
    this->assertDispatchCount(numCommands);

    // Nominal execution raises no warnings.
    ASSERT_EVENTS_InvalidSequence_SIZE(0);
    ASSERT_EVENTS_CommandFailed_SIZE(0);
    ASSERT_EVENTS_DroppingStrayResponse_SIZE(0);
}

void SeqTester ::testRunCommandPendsUntilActive() {
    REQUIREMENT("MFPM-ACT-004");
    // A single command.
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addCommand(OPCODE_A, &arg, sizeof(arg));
    this->configureWith(b.data(), b.len());
    this->setNow(500);

    // RUN only latches a pending request; it must not dispatch yet.
    this->clearHistory();
    this->sendCmd_RUN(0, 0, SeqNames::STARTUP);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Seq::OPCODE_RUN, 0, Fw::CmdResponse::OK);
    this->assertDispatchCount(0);

    // The next idle activeIn tick picks up the pending run and dispatches.
    this->tick();
    this->assertDispatchCount(1);
    this->assertDispatch(0, OPCODE_A, 5, 0);
}

void SeqTester ::testImmediateDispatchOrder() {
    REQUIREMENT("MFPM-ACT-001");
    REQUIREMENT("MFPM-ACT-002");
    // Three back-to-back commands with distinct args.
    ActionBuilder b;
    const U8 a0 = 0xA0, a1 = 0xA1, a2 = 0xA2;
    b.addCommand(OPCODE_A, &a0, 1);
    b.addCommand(OPCODE_A, &a1, 1);
    b.addCommand(OPCODE_A, &a2, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    // One command per activeIn tick, each gated by a command response. Commands
    // are cmdSize(4) + opcode(4) + arg(1) = 9 bytes on the wire.
    const U32 stride = 9;
    for (U32 i = 0; i < 3; i++) {
        this->tick();
        this->assertDispatchCount(i + 1);
        this->assertDispatch(i, OPCODE_A, 5, i * stride);

        // Verify the argument byte survived the copy into the ComBuffer.
        const Fw::ComBuffer& data = this->fromPortHistory_commandOut->at(i).data;
        ASSERT_EQ(data.getSize(), 5u);
        ASSERT_EQ(data.getBuffAddr()[4], static_cast<U8>(0xA0 + i));

        this->respondLast(Fw::CmdResponse::OK);
    }

    this->tick();
    this->assertDispatchCount(3);
}

void SeqTester ::testCommandFailureAborts() {
    REQUIREMENT("MFPM-ACT-003");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    b.addCommand(OPCODE_B, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    // First command dispatches.
    this->tick();
    this->assertDispatchCount(1);

    // Its command fails: the sequence must abort and dispatch nothing further.
    this->respondLast(Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CommandFailed_SIZE(1);
    this->tick();
    this->sched();
    this->assertDispatchCount(1);
}

void SeqTester ::testStrayResponseDropped() {
    this->configureWith(fprime_seq_GENERIC, fprime_seq_GENERIC_len);
    this->setNow(0);

    // No sequence running: a command response is stray and should be dropped
    // with a warning, not acted upon.
    this->clearHistory();
    const FwOpcodeType strayOpcode = 0x1234;
    this->invoke_to_commandResponseIn(0, strayOpcode, 7, Fw::CmdResponse::OK);
    ASSERT_EVENTS_DroppingStrayResponse_SIZE(1);
    ASSERT_EVENTS_DroppingStrayResponse(0, strayOpcode, 7, Fw::CmdResponse::OK);
    this->assertDispatchCount(0);
}

void SeqTester ::testPendingRunAfterCompletion() {
    REQUIREMENT("MFPM-ACT-004");
    // A one-command sequence.
    ActionBuilder b;
    const U8 arg = 0x55;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    // Start it.
    this->run();
    this->tick();
    this->assertDispatchCount(1);

    // While AWAITING_RESPONSE, issue a second RUN. It must be accepted (latched)
    // but not start yet.
    this->sendCmd_RUN(0, 1, SeqNames::STARTUP);
    this->tick();
    this->assertDispatchCount(1);  // still gated on the outstanding response

    // Complete the first run.
    this->respondLast(Fw::CmdResponse::OK);

    // The next idle tick picks up the latched RUN and re-dispatches the sequence.
    this->tick();
    this->assertDispatchCount(2);
    this->assertDispatch(1, OPCODE_A, 5, 0);
}

void SeqTester ::testInvalidCmdSizeFails() {
    REQUIREMENT("MFPM-ACT-008");
    // A cmdSize smaller than a bare opcode, but with enough trailing bytes that
    // it does NOT overrun the buffer. This isolates the lower-bound check
    // (cmdSize < sizeof(FwOpcodeType)) from the overrun check.
    ActionBuilder b;
    b.addU32(2);      // cmdSize = 2  (< sizeof(FwOpcodeType) == 4)
    b.addByte(0xDE);  // trailing payload byte 1 (so cmdSize does not overrun)
    b.addByte(0xAD);  // trailing payload byte 2
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    this->clearHistory();
    this->tick();

    this->assertDispatchCount(0);
    ASSERT_EVENTS_InvalidSequence_SIZE(1);
    ASSERT_EVENTS_InvalidSequence(0, 2, 0);
}

void SeqTester ::testCmdSizeOverrunsBufferFails() {
    REQUIREMENT("MFPM-ACT-008");
    // A cmdSize that claims more bytes than remain in the action buffer.
    ActionBuilder b;
    b.addU32(0x40);  // cmdSize = 64, far more than what follows
    b.addByte(0x00);
    b.addByte(0x00);
    b.addByte(0x00);
    b.addByte(0x20);  // only 4 payload bytes present
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    this->clearHistory();
    this->tick();

    this->assertDispatchCount(0);
    ASSERT_EVENTS_InvalidSequence_SIZE(1);
    ASSERT_EVENTS_InvalidSequence(0, 0x40, 0);
}

void SeqTester ::testEmptyAction() {
    // A zero-length action is an empty (but valid) sequence: it finishes
    // immediately without dispatching or warning.
    static const U8 empty[1] = {0};
    this->configureWith(empty, 0);
    this->setNow(0);
    this->run();

    this->clearHistory();
    this->tick();

    this->assertDispatchCount(0);
    ASSERT_EVENTS_InvalidSequence_SIZE(0);
    ASSERT_EVENTS_FinishedSequence_SIZE(1);
}

void SeqTester ::testTruncatedCmdSizeFails() {
    REQUIREMENT("MFPM-ACT-008");
    // Only 2 of the 4 cmdSize bytes are present, so the U32 read fails.
    ActionBuilder b;
    b.addByte(0x00);
    b.addByte(0x00);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    this->clearHistory();
    this->tick();

    this->assertDispatchCount(0);
    ASSERT_EVENTS_InvalidSequence_SIZE(1);
}

// ----------------------------------------------------------------------
// WAIT_TICKS
// ----------------------------------------------------------------------

void SeqTester ::testWaitTicksZeroResumesNextTick() {
    REQUIREMENT("MFPM-ACT-006");
    // A WAIT_TICKS(0) directive followed by one command.
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addWaitTicks();
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    // The WAIT_TICKS command dispatches like any other; the round-trip through
    // the dispatcher arms the sleep with n == 0.
    this->tick();
    this->assertDispatchCount(1);
    this->completeWaitTicks(0);

    // n == 0 resumes on the very next rate-group tick.
    this->sched();
    this->assertDispatchCount(2);
    this->assertDispatch(1, OPCODE_A, 5, WAIT_TICKS_STRIDE);
}

void SeqTester ::testWaitTicksBlocksNTicks() {
    REQUIREMENT("MFPM-ACT-006");
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addWaitTicks();
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    this->tick();
    this->assertDispatchCount(1);
    this->completeWaitTicks(2);

    // Two rate-group ticks pass without resuming.
    this->sched();
    this->assertDispatchCount(1);
    this->sched();
    this->assertDispatchCount(1);

    // The third tick releases the following command.
    this->sched();
    this->assertDispatchCount(2);
    this->assertDispatch(1, OPCODE_A, 5, WAIT_TICKS_STRIDE);
}

void SeqTester ::testWaitTicksAtEndOfSequence() {
    REQUIREMENT("MFPM-ACT-006");
    // A WAIT_TICKS as the LAST command in the sequence: the wait advances the
    // program counter to the end of the buffer, so waking must finish cleanly
    // rather than deserializing a zero-length record off the end (which would
    // raise a spurious InvalidSequence). Regression for offset-at-end.
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addCommand(OPCODE_A, &arg, 1);
    b.addWaitTicks();
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    // Dispatch and complete the leading command.
    this->tick();
    this->assertDispatchCount(1);
    this->respondLast(Fw::CmdResponse::OK);

    // Dispatch the trailing WAIT_TICKS and arm the sleep.
    this->tick();
    this->assertDispatchCount(2);
    this->completeWaitTicks(0);

    // Waking at the end of the buffer must finish the sequence, not dispatch or
    // warn.
    this->clearHistory();
    this->sched();
    this->assertDispatchCount(0);
    ASSERT_EVENTS_InvalidSequence_SIZE(0);
    ASSERT_EVENTS_FinishedSequence_SIZE(1);
}

// ----------------------------------------------------------------------
// WAIT_UNTIL
// ----------------------------------------------------------------------

void SeqTester ::testWaitUntilSleepsUntilTime() {
    REQUIREMENT("MFPM-ACT-006");
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addWaitUntil();
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(100);
    this->run();

    this->tick();
    this->assertDispatchCount(1);
    this->completeWaitUntil(TEST_TIME_BASE, 200, 0);

    // Before the requested time, schedIn keeps sleeping.
    this->setNow(150);
    this->sched();
    this->assertDispatchCount(1);
    this->setNow(199);
    this->sched();
    this->assertDispatchCount(1);

    // At the requested time, the following command is released.
    this->setNow(200);
    this->sched();
    this->assertDispatchCount(2);
    this->assertDispatch(1, OPCODE_A, 5, WAIT_UNTIL_STRIDE);
}

void SeqTester ::testWaitUntilWakesOnExactTime() {
    REQUIREMENT("MFPM-ACT-006");
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addWaitUntil();
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNowUs(10, 0);
    this->run();

    this->tick();
    this->assertDispatchCount(1);
    this->completeWaitUntil(TEST_TIME_BASE, 10, 500000);  // wake at 10.500000s

    // One microsecond early stays asleep; the exact time releases the command.
    this->setNowUs(10, 499999);
    this->sched();
    this->assertDispatchCount(1);
    this->setNowUs(10, 500000);
    this->sched();
    this->assertDispatchCount(2);
    this->assertDispatch(1, OPCODE_A, 5, WAIT_UNTIL_STRIDE);
}

void SeqTester ::testWaitUntilPastResumesImmediately() {
    REQUIREMENT("MFPM-ACT-006");
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addWaitUntil();
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(1000);
    this->run();

    this->tick();
    this->assertDispatchCount(1);
    this->completeWaitUntil(TEST_TIME_BASE, 500, 0);  // already in the past

    // The next schedIn observes the time already reached and resumes.
    this->sched();
    this->assertDispatchCount(2);
    this->assertDispatch(1, OPCODE_A, 5, WAIT_UNTIL_STRIDE);
}

void SeqTester ::testWaitUntilMismatchedBaseNeverWakes() {
    REQUIREMENT("MFPM-ACT-006");
    ActionBuilder b;
    const U8 arg = 0x20;
    b.addWaitUntil();
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(1000);
    this->run();

    this->tick();
    this->assertDispatchCount(1);
    // Wait on a base the FSW clock never reports, with a time already in the past.
    this->completeWaitUntil(TimeBase::TB_WORKSTATION_TIME, 0, 0);

    // Even well past the requested time, the base never matches, so it stays asleep.
    this->setNow(5000);
    this->sched();
    this->sched();
    this->assertDispatchCount(1);
}

// ----------------------------------------------------------------------
// RUN_ON_ERROR
// ----------------------------------------------------------------------

void SeqTester ::testRunOnErrorStartsRecovery() {
    REQUIREMENT("MFPM-ACT-005");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    b.addCommand(OPCODE_B, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    // Arm the recovery table, then start the main sequence.
    this->sendCmd_RUN_ON_ERROR(0, 0, SeqNames::SENSORS);
    this->sendCmd_RUN(0, 1, SeqNames::STARTUP);

    // First command dispatches, then fails.
    this->tick();
    this->assertDispatchCount(1);
    this->respondLast(Fw::CmdResponse::EXECUTION_ERROR);
    ASSERT_EVENTS_CommandFailed_SIZE(1);

    // The next idle tick starts the armed recovery table.
    this->clearHistory();
    this->tick();
    ASSERT_EVENTS_StartingSequence_SIZE(1);
    ASSERT_EVENTS_StartingSequence(0, SeqNames::SENSORS);
    this->assertDispatchCount(1);
    this->assertDispatch(0, OPCODE_A, 5, 0);
}

void SeqTester ::testRunOnErrorNotTriggeredOnSuccess() {
    REQUIREMENT("MFPM-ACT-005");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    this->sendCmd_RUN_ON_ERROR(0, 0, SeqNames::SENSORS);
    this->sendCmd_RUN(0, 1, SeqNames::STARTUP);

    // Run the sequence to successful completion.
    this->tick();
    this->assertDispatchCount(1);
    this->respondLast(Fw::CmdResponse::OK);
    ASSERT_EVENTS_FinishedSequence_SIZE(1);

    // No recovery is pending: the next ticks dispatch nothing.
    this->clearHistory();
    this->tick();
    this->tick();
    this->assertDispatchCount(0);
    ASSERT_EVENTS_StartingSequence_SIZE(0);
}

void SeqTester ::testRunOnErrorClearsPendingRun() {
    REQUIREMENT("MFPM-ACT-005");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    // Latch a pending RUN, then RUN_ON_ERROR, which clears it.
    this->sendCmd_RUN(0, 0, SeqNames::STARTUP);
    this->sendCmd_RUN_ON_ERROR(0, 1, SeqNames::SENSORS);

    // The previously-pending RUN must not start on the next activeIn.
    this->clearHistory();
    this->tick();
    this->tick();
    this->assertDispatchCount(0);
    ASSERT_EVENTS_StartingSequence_SIZE(0);
}

void SeqTester ::testRunOnErrorIsOneShot() {
    REQUIREMENT("MFPM-ACT-005");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    this->sendCmd_RUN_ON_ERROR(0, 0, SeqNames::SENSORS);
    this->sendCmd_RUN(0, 1, SeqNames::STARTUP);

    // First run fails, arming the recovery run.
    this->tick();
    this->respondLast(Fw::CmdResponse::EXECUTION_ERROR);

    // Recovery starts; it also fails. Because RUN_ON_ERROR is one-shot, the
    // recovery must NOT re-arm itself.
    this->tick();
    this->assertDispatchCount(2);
    this->respondLast(Fw::CmdResponse::EXECUTION_ERROR);

    // No further recovery is pending.
    this->clearHistory();
    this->tick();
    this->tick();
    this->assertDispatchCount(0);
    ASSERT_EVENTS_StartingSequence_SIZE(0);
}

// ----------------------------------------------------------------------
// CANCEL
// ----------------------------------------------------------------------

void SeqTester ::testCancelWhileSleeping() {
    REQUIREMENT("MFPM-ACT-007");
    // A sequence that waits.
    ActionBuilder b;
    b.addWaitTicks();
    this->configureWith(b.data(), b.len());
    this->setNow(100);
    this->run();

    // Dispatch the wait and arm the sleep.
    this->tick();
    this->assertDispatchCount(1);
    this->completeWaitTicks(5);

    // CANCEL stops the sleep and emits a Cancelled event. The program counter
    // advanced past the WAIT command (offset == WAIT_TICKS_STRIDE).
    this->clearHistory();
    this->sendCmd_CANCEL(0, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Seq::OPCODE_CANCEL, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_Cancelled_SIZE(1);
    ASSERT_EVENTS_Cancelled(0, SeqNames::STARTUP, WAIT_TICKS_STRIDE);

    // Even past the original wake, schedIn must not dispatch anything.
    this->advance(100);
    this->sched();
    this->sched();
    this->assertDispatchCount(0);
}

void SeqTester ::testCancelWhileAwaitingResponse() {
    REQUIREMENT("MFPM-ACT-007");
    // Two commands; we cancel after the first is dispatched but before its
    // response arrives.
    ActionBuilder b;
    const U8 arg = 0x22;
    b.addCommand(OPCODE_A, &arg, 1);
    b.addCommand(OPCODE_B, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    // First command dispatched; component is AWAITING_RESPONSE.
    this->tick();
    this->assertDispatchCount(1);
    const U32 dispatchedCmdSeq = this->fromPortHistory_commandOut->at(0).context;

    // CANCEL now. The program counter advanced past command 0 (to offset 9),
    // so the Cancelled event reports that offset.
    this->clearHistory();
    this->sendCmd_CANCEL(0, 0);
    ASSERT_EVENTS_Cancelled_SIZE(1);
    ASSERT_EVENTS_Cancelled(0, SeqNames::STARTUP, 9);

    // The in-flight command's (late) response is now stray: it must be dropped,
    // and must NOT advance to the second command.
    this->clearHistory();
    this->invoke_to_commandResponseIn(0, OPCODE_A, dispatchedCmdSeq, Fw::CmdResponse::OK);
    ASSERT_EVENTS_DroppingStrayResponse_SIZE(1);

    // No further ticks dispatch anything.
    this->tick();
    this->sched();
    this->assertDispatchCount(0);
}

void SeqTester ::testCancelClearsPendingRun() {
    REQUIREMENT("MFPM-ACT-007");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    // Latch a pending RUN but do not start it (no activeIn yet).
    this->run();

    // CANCEL clears the pending request.
    this->sendCmd_CANCEL(0, 0);

    // The next activeIn must NOT start the previously-pending run.
    this->clearHistory();
    this->tick();
    this->tick();
    this->assertDispatchCount(0);
}

void SeqTester ::testCancelResetsToStart() {
    REQUIREMENT("MFPM-ACT-007");
    // Two commands.
    ActionBuilder b;
    const U8 a0 = 0xA0, a1 = 0xA1;
    b.addCommand(OPCODE_A, &a0, 1);
    b.addCommand(OPCODE_B, &a1, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);
    this->run();

    // Execute the first command so the program counter advances off zero.
    this->tick();
    this->assertDispatchCount(1);
    this->assertDispatch(0, OPCODE_A, 5, 0);
    this->respondLast(Fw::CmdResponse::OK);

    // CANCEL mid-sequence.
    this->sendCmd_CANCEL(0, 0);

    // A fresh RUN must restart from the FIRST command (offset 0), not resume at
    // the second command.
    this->clearHistory();
    this->sendCmd_RUN(0, 1, SeqNames::STARTUP);
    this->tick();
    this->assertDispatchCount(1);
    this->assertDispatch(0, OPCODE_A, 5, 0);
}

void SeqTester ::testCancelWhileIdleNoOp() {
    REQUIREMENT("MFPM-ACT-007");
    ActionBuilder b;
    const U8 arg = 0x44;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    // CANCEL with nothing running or pending: succeeds and changes nothing.
    this->clearHistory();
    this->sendCmd_CANCEL(0, 0);
    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, Seq::OPCODE_CANCEL, 0, Fw::CmdResponse::OK);
    ASSERT_EVENTS_Cancelled_SIZE(1);
    this->assertDispatchCount(0);

    // A later RUN still works normally.
    this->sendCmd_RUN(0, 1, SeqNames::STARTUP);
    this->tick();
    this->assertDispatchCount(1);
    this->assertDispatch(0, OPCODE_A, 5, 0);
}

void SeqTester ::testCancelClearsRunOnError() {
    REQUIREMENT("MFPM-ACT-007");
    ActionBuilder b;
    const U8 arg = 0x33;
    b.addCommand(OPCODE_A, &arg, 1);
    this->configureWith(b.data(), b.len());
    this->setNow(0);

    // Arm a recovery table, then CANCEL, which must disarm it.
    this->sendCmd_RUN_ON_ERROR(0, 0, SeqNames::SENSORS);
    this->sendCmd_CANCEL(0, 1);

    // Run a sequence and fail it: no recovery should be triggered.
    this->sendCmd_RUN(0, 2, SeqNames::STARTUP);
    this->tick();
    this->assertDispatchCount(1);
    this->respondLast(Fw::CmdResponse::EXECUTION_ERROR);

    this->clearHistory();
    this->tick();
    this->tick();
    this->assertDispatchCount(0);
    ASSERT_EVENTS_StartingSequence_SIZE(0);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void SeqTester ::configureWith(const U8* data, U32 len) {
    for (U32 i = 0; i < SeqNames::NUM_CONSTANTS; i++) {
        this->m_actions[i] = Seq::Action(data, len);
    }
    this->component.configure(this->m_actions);
}

void SeqTester ::setNow(U32 seconds) {
    this->setNowUs(seconds, 0);
}

void SeqTester ::setNowUs(U32 seconds, U32 useconds) {
    this->m_nowSeconds = seconds;
    this->setTestTime(Fw::Time(TEST_TIME_BASE, seconds, useconds));
}

void SeqTester ::advance(U32 deltaSeconds) {
    this->setNow(this->m_nowSeconds + deltaSeconds);
}

void SeqTester ::run() {
    this->sendCmd_RUN(0, 0, SeqNames::STARTUP);
}

void SeqTester ::tick() {
    // activeIn now returns bool ("did work"), but this helper is invoked from
    // many call sites with differing expected component state (idle, pending,
    // just-completed, etc.), so the return value is not uniformly meaningful
    // here. Callers assert observable effects (dispatch count/content)
    // instead of this return value.
    this->invoke_to_activeIn(0, STest::Pick::any());
}

void SeqTester ::sched() {
    this->invoke_to_schedIn(0, STest::Pick::any());
}

void SeqTester ::respondLast(const Fw::CmdResponse& response) {
    const U32 n = this->fromPortHistory_commandOut->size();
    ASSERT_GT(n, 0u) << "respondLast called with nothing dispatched";
    const auto& e = this->fromPortHistory_commandOut->at(n - 1);
    // Seq passes the command offset as the Com port context / cmdSeq.
    this->invoke_to_commandResponseIn(0, OPCODE_A, e.context, response);
}

void SeqTester ::completeWaitTicks(U32 n) {
    // Simulate the dispatcher round-trip for a WAIT_TICKS command the component
    // just dispatched out commandOut: run the command handler (which arms the
    // sleep) then route the OK response back to commandResponseIn -- exactly the
    // re-entrant sequence the flight command dispatcher produces.
    const U32 offset = this->lastDispatchOffset();
    this->sendCmd_WAIT_TICKS(0, 0, n);
    this->invoke_to_commandResponseIn(0, Seq::OPCODE_WAIT_TICKS, offset, Fw::CmdResponse::OK);
}

void SeqTester ::completeWaitUntil(const TimeBase& base, U32 seconds, U32 useconds) {
    // See completeWaitTicks: simulate the WAIT_UNTIL dispatcher round-trip.
    const U32 offset = this->lastDispatchOffset();
    this->sendCmd_WAIT_UNTIL(0, 0, base, seconds, useconds);
    this->invoke_to_commandResponseIn(0, Seq::OPCODE_WAIT_UNTIL, offset, Fw::CmdResponse::OK);
}

U32 SeqTester ::lastDispatchOffset() {
    const U32 n = this->fromPortHistory_commandOut->size();
    FW_ASSERT(n > 0);
    return this->fromPortHistory_commandOut->at(n - 1).context;
}

void SeqTester ::assertDispatchCount(U32 n) {
    ASSERT_from_commandOut_SIZE(n);
}

void SeqTester ::assertDispatch(U32 index, FwOpcodeType opcode, U32 totalSize, U32 cmdSeq) {
    ASSERT_GT(this->fromPortHistory_commandOut->size(), index);
    const auto& e = this->fromPortHistory_commandOut->at(index);

    // The command sequence / context is the command's byte offset.
    ASSERT_EQ(e.context, cmdSeq) << "dispatch " << index << " cmdSeq mismatch";

    // The ComBuffer holds (opcode || args) with the recorded cmdSize.
    const Fw::ComBuffer& data = e.data;
    ASSERT_EQ(static_cast<U32>(data.getSize()), totalSize) << "dispatch " << index << " size mismatch";

    // Opcode is the first 4 bytes, big-endian.
    const U8* d = data.getBuffAddr();
    const U32 decodedOpcode =
        (static_cast<U32>(d[0]) << 24) | (static_cast<U32>(d[1]) << 16) | (static_cast<U32>(d[2]) << 8) | d[3];
    ASSERT_EQ(decodedOpcode, static_cast<U32>(opcode)) << "dispatch " << index << " opcode mismatch";
}

// ----------------------------------------------------------------------
// ActionBuilder
// ----------------------------------------------------------------------

void SeqTester ::ActionBuilder ::putU8(U8 v) {
    FW_ASSERT(m_len < CAP);
    m_buf[m_len++] = v;
}

void SeqTester ::ActionBuilder ::putU32(U32 v) {
    putU8(static_cast<U8>((v >> 24) & 0xFF));
    putU8(static_cast<U8>((v >> 16) & 0xFF));
    putU8(static_cast<U8>((v >> 8) & 0xFF));
    putU8(static_cast<U8>(v & 0xFF));
}

void SeqTester ::ActionBuilder ::addCommand(FwOpcodeType opcode, const U8* args, U32 argLen) {
    putU32(4 + argLen);  // cmdSize = opcode + args
    putU32(static_cast<U32>(opcode));
    for (U32 i = 0; i < argLen; i++) {
        putU8(args[i]);
    }
}

void SeqTester ::ActionBuilder ::addWaitTicks() {
    // A WAIT_TICKS command buffer: opcode + a 4-byte U32 arg. Its actual arg
    // value is irrelevant here -- the test drives n through completeWaitTicks().
    const U8 arg[4] = {0, 0, 0, 0};
    addCommand(Seq::OPCODE_WAIT_TICKS, arg, sizeof(arg));
}

void SeqTester ::ActionBuilder ::addWaitUntil() {
    // A WAIT_UNTIL command buffer: opcode + (timeBase, seconds, useconds). The
    // payload bytes are placeholders -- the test drives the values through
    // completeWaitUntil().
    const U8 arg[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    addCommand(Seq::OPCODE_WAIT_UNTIL, arg, sizeof(arg));
}

void SeqTester ::ActionBuilder ::addByte(U8 b) {
    putU8(b);
}

void SeqTester ::ActionBuilder ::addU32(U32 v) {
    putU32(v);
}

}  // namespace Samd21

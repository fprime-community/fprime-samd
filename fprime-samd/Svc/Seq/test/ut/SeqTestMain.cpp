// ======================================================================
// \title  SeqTestMain.cpp
// \author tumbar
// \brief  cpp file for Seq component test main function
// ======================================================================

#include "SeqTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "STest/Random/Random.hpp"

TEST(SequenceExecution, GenericSequence) {
    COMMENT("Execute the GENERIC action end to end, verifying command order, args, and byte offsets.");
    Samd21::SeqTester tester;
    tester.testGenericSequence();
}

TEST(SequenceExecution, RunCommandPendsUntilActive) {
    COMMENT("RUN latches a pending request that only starts on the next idle activeIn tick.");
    Samd21::SeqTester tester;
    tester.testRunCommandPendsUntilActive();
}

TEST(SequenceExecution, ImmediateDispatchOrder) {
    COMMENT("Back-to-back commands dispatch one per activeIn tick, gated by command responses.");
    Samd21::SeqTester tester;
    tester.testImmediateDispatchOrder();
}

TEST(SequenceExecution, CommandFailureAborts) {
    COMMENT("A failing command response aborts the remainder of the sequence.");
    Samd21::SeqTester tester;
    tester.testCommandFailureAborts();
}

TEST(SequenceExecution, StrayResponseDropped) {
    COMMENT("A command response received while idle is dropped and logged.");
    Samd21::SeqTester tester;
    tester.testStrayResponseDropped();
}

TEST(SequenceExecution, PendingRunAfterCompletion) {
    COMMENT("A RUN issued mid-sequence is latched and runs after the current sequence completes.");
    Samd21::SeqTester tester;
    tester.testPendingRunAfterCompletion();
}

TEST(SequenceExecution, InvalidCmdSizeFails) {
    COMMENT("A command whose cmdSize is smaller than a bare opcode emits InvalidSequence.");
    Samd21::SeqTester tester;
    tester.testInvalidCmdSizeFails();
}

TEST(SequenceExecution, CmdSizeOverrunsBufferFails) {
    COMMENT("A command whose cmdSize runs past the action buffer emits InvalidSequence.");
    Samd21::SeqTester tester;
    tester.testCmdSizeOverrunsBufferFails();
}

TEST(SequenceExecution, EmptyAction) {
    COMMENT("A zero-length action is a valid empty sequence: it finishes immediately with no warning.");
    Samd21::SeqTester tester;
    tester.testEmptyAction();
}

TEST(SequenceExecution, TruncatedCmdSizeFails) {
    COMMENT("A command truncated within the cmdSize field emits InvalidSequence.");
    Samd21::SeqTester tester;
    tester.testTruncatedCmdSizeFails();
}

TEST(WaitTicks, ZeroResumesNextTick) {
    COMMENT("WAIT_TICKS(0) resumes the sequence on the very next rate-group tick.");
    Samd21::SeqTester tester;
    tester.testWaitTicksZeroResumesNextTick();
}

TEST(WaitTicks, BlocksNTicks) {
    COMMENT("WAIT_TICKS(n) blocks for exactly n rate-group ticks, then resumes.");
    Samd21::SeqTester tester;
    tester.testWaitTicksBlocksNTicks();
}

TEST(WaitTicks, AtEndOfSequence) {
    COMMENT("A WAIT_TICKS at the end of a sequence finishes cleanly without a spurious InvalidSequence.");
    Samd21::SeqTester tester;
    tester.testWaitTicksAtEndOfSequence();
}

TEST(WaitUntil, SleepsUntilTime) {
    COMMENT("WAIT_UNTIL sleeps until the FSW clock reaches the requested time.");
    Samd21::SeqTester tester;
    tester.testWaitUntilSleepsUntilTime();
}

TEST(WaitUntil, WakesOnExactTime) {
    COMMENT("WAIT_UNTIL wakes when the FSW clock exactly equals the requested time.");
    Samd21::SeqTester tester;
    tester.testWaitUntilWakesOnExactTime();
}

TEST(WaitUntil, PastResumesImmediately) {
    COMMENT("WAIT_UNTIL whose requested time is already past resumes on the next schedIn tick.");
    Samd21::SeqTester tester;
    tester.testWaitUntilPastResumesImmediately();
}

TEST(WaitUntil, MismatchedBaseNeverWakes) {
    COMMENT("WAIT_UNTIL on a time base the FSW clock never reports never wakes.");
    Samd21::SeqTester tester;
    tester.testWaitUntilMismatchedBaseNeverWakes();
}

TEST(RunOnError, StartsRecovery) {
    COMMENT("RUN_ON_ERROR arms a recovery table that starts after a command failure.");
    Samd21::SeqTester tester;
    tester.testRunOnErrorStartsRecovery();
}

TEST(RunOnError, NotTriggeredOnSuccess) {
    COMMENT("RUN_ON_ERROR does not trigger when the sequence completes successfully.");
    Samd21::SeqTester tester;
    tester.testRunOnErrorNotTriggeredOnSuccess();
}

TEST(RunOnError, ClearsPendingRun) {
    COMMENT("Issuing RUN_ON_ERROR clears any pending normal RUN.");
    Samd21::SeqTester tester;
    tester.testRunOnErrorClearsPendingRun();
}

TEST(RunOnError, IsOneShot) {
    COMMENT("The armed recovery is one-shot: a second failure does not re-trigger it.");
    Samd21::SeqTester tester;
    tester.testRunOnErrorIsOneShot();
}

TEST(Cancel, WhileSleeping) {
    COMMENT("CANCEL while a WAIT is sleeping stops the sleep; schedIn no longer dispatches.");
    Samd21::SeqTester tester;
    tester.testCancelWhileSleeping();
}

TEST(Cancel, WhileAwaitingResponse) {
    COMMENT("CANCEL while awaiting a response stops execution; the late response is dropped as stray.");
    Samd21::SeqTester tester;
    tester.testCancelWhileAwaitingResponse();
}

TEST(Cancel, ClearsPendingRun) {
    COMMENT("CANCEL clears a pending RUN so it does not start on the next activeIn.");
    Samd21::SeqTester tester;
    tester.testCancelClearsPendingRun();
}

TEST(Cancel, ResetsToStart) {
    COMMENT("A RUN after CANCEL re-executes the sequence from the first command.");
    Samd21::SeqTester tester;
    tester.testCancelResetsToStart();
}

TEST(Cancel, WhileIdleNoOp) {
    COMMENT("CANCEL while idle with nothing pending is a harmless no-op.");
    Samd21::SeqTester tester;
    tester.testCancelWhileIdleNoOp();
}

TEST(Cancel, ClearsRunOnError) {
    COMMENT("CANCEL clears an armed RUN_ON_ERROR recovery table.");
    Samd21::SeqTester tester;
    tester.testCancelClearsRunOnError();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    STest::Random::seed();
    return RUN_ALL_TESTS();
}

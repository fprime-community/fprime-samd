// ======================================================================
// \title  SystemInfoTester.cpp
// \author tumbar
// \brief  cpp file for SystemInfo test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/SystemInfo/test/ut/SystemInfoTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "fprime-samd/Drv/SystemInfo/SystemInfoVersion.hpp"

namespace Samd21 {

//! Every reset reason the HAL can report. Kept exhaustive on purpose: a new ResetReason
//! value should make this list, and the tests that walk it, obviously incomplete.
static const ResetReason::T ALL_RESET_REASONS[] = {
    ResetReason::POWER_ON, ResetReason::BROWN_OUT_12,   ResetReason::BROWN_OUT_33, ResetReason::EXTERNAL,
    ResetReason::SYSTEM,   ResetReason::WATCHDOG_TIMER, ResetReason::UNKNOWN,
};

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

SystemInfoTester::SystemInfoTester()
    : SystemInfoGTestBase("SystemInfoTester", SystemInfoTester::MAX_HISTORY_SIZE), component("SystemInfo") {
    this->initComponents();
    this->connectPorts();
}

SystemInfoTester::~SystemInfoTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void SystemInfoTester::resetTest() {
    this->clearHistory();
    SystemInfoHardware::resetSystemInfoState();
}

void SystemInfoTester::assertEmitReports(ResetReason reason, U32 cmdSeq) {
    SystemInfoHardware::setResetReason(reason);

    this->sendCmd_EMIT_SYSTEM_INFO(SystemInfoTester::TEST_INSTANCE_ID, cmdSeq);

    ASSERT_EVENTS_SIZE(1);
    ASSERT_EVENTS_SystemInfo_SIZE(1);
    ASSERT_EVENTS_SystemInfo(0, reason, SystemInfoVersion::PROJECT_COMMIT, SystemInfoVersion::FPRIME_COMMIT,
                             SystemInfoVersion::SAMD_COMMIT);

    // One update per channel; the struct is reported field by field because it does not
    // fit FW_TLM_BUFFER_MAX_SIZE as a whole
    ASSERT_TLM_SIZE(4);
    ASSERT_TLM_ResetCause_SIZE(1);
    ASSERT_TLM_ResetCause(0, reason);
    ASSERT_TLM_ProjectCommit_SIZE(1);
    ASSERT_TLM_ProjectCommit(0, SystemInfoVersion::PROJECT_COMMIT);
    ASSERT_TLM_FprimeCommit_SIZE(1);
    ASSERT_TLM_FprimeCommit(0, SystemInfoVersion::FPRIME_COMMIT);
    ASSERT_TLM_SamdCommit_SIZE(1);
    ASSERT_TLM_SamdCommit(0, SystemInfoVersion::SAMD_COMMIT);

    ASSERT_CMD_RESPONSE_SIZE(1);
    ASSERT_CMD_RESPONSE(0, SystemInfoComponentBase::OPCODE_EMIT_SYSTEM_INFO, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void SystemInfoTester::testEmitSystemInfoReportsResetReason() {
    REQUIREMENT("SAMD21-HEALTH-001: SystemInfo shall report the cause of the most recent reset on command");

    // The reason the HAL yields must reach the event, the channel and a successful command
    // response, for every reason the HAL can produce
    for (const auto reason : ALL_RESET_REASONS) {
        this->resetTest();
        this->assertEmitReports(reason, SystemInfoTester::TEST_CMD_SEQ);
    }
}

void SystemInfoTester::testCommitStampsAreReported() {
    REQUIREMENT("SAMD21-HEALTH-002: SystemInfo shall report the build commits of the project, fprime and fprime-samd");

    this->resetTest();
    this->assertEmitReports(ResetReason::POWER_ON, SystemInfoTester::TEST_CMD_SEQ);

    // assertEmitReports checks all four event arguments at once; pull the commits out
    // individually so a mismatch names the offending field
    const auto& reported = this->eventHistory_SystemInfo->at(0);
    ASSERT_EQ(reported.projectCommit, SystemInfoVersion::PROJECT_COMMIT);
    ASSERT_EQ(reported.fprimeCommit, SystemInfoVersion::FPRIME_COMMIT);
    ASSERT_EQ(reported.samdCommit, SystemInfoVersion::SAMD_COMMIT);
}

void SystemInfoTester::testHardwareQueriedOncePerCommand() {
    this->resetTest();

    ASSERT_EQ(SystemInfoHardware::getSystemInfoState().get_reset_reason_count, 0u);

    // The component must read the reset cause fresh on each command rather than caching it
    // at construction, so the count tracks the command count exactly
    for (U32 expected_count = 1; expected_count <= 3; expected_count++) {
        this->sendCmd_EMIT_SYSTEM_INFO(SystemInfoTester::TEST_INSTANCE_ID, expected_count);
        ASSERT_EQ(SystemInfoHardware::getSystemInfoState().get_reset_reason_count, expected_count);
    }
}

void SystemInfoTester::testRepeatedEmit() {
    this->resetTest();

    // Each command appends one event, one channel update and one response, and the
    // sequence number is echoed back per command
    const U32 command_count = 3;
    for (U32 i = 0; i < command_count; i++) {
        SystemInfoHardware::setResetReason(ResetReason::EXTERNAL);
        this->sendCmd_EMIT_SYSTEM_INFO(SystemInfoTester::TEST_INSTANCE_ID, i);
    }

    ASSERT_EVENTS_SystemInfo_SIZE(command_count);
    ASSERT_TLM_ResetCause_SIZE(command_count);
    ASSERT_TLM_ProjectCommit_SIZE(command_count);
    ASSERT_CMD_RESPONSE_SIZE(command_count);

    for (U32 i = 0; i < command_count; i++) {
        ASSERT_EVENTS_SystemInfo(i, ResetReason::EXTERNAL, SystemInfoVersion::PROJECT_COMMIT,
                                 SystemInfoVersion::FPRIME_COMMIT, SystemInfoVersion::SAMD_COMMIT);
        ASSERT_TLM_ResetCause(i, ResetReason::EXTERNAL);
        ASSERT_TLM_ProjectCommit(i, SystemInfoVersion::PROJECT_COMMIT);
        ASSERT_CMD_RESPONSE(i, SystemInfoComponentBase::OPCODE_EMIT_SYSTEM_INFO, i, Fw::CmdResponse::OK);
    }
}

}  // namespace Samd21

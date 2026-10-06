// ======================================================================
// \title  SystemInfo.cpp
// \author tumbar
// \brief  cpp file for SystemInfo component implementation class
// ======================================================================

#include "fprime-samd/Drv/SystemInfo/SystemInfo.hpp"
#include "fprime-samd/Drv/SystemInfo/SystemInfoHardware.hpp"
#include "fprime-samd/Drv/SystemInfo/SystemInfoVersion.hpp"
#include "fprime-samd/Drv/Types/ResetReasonEnumAc.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SystemInfo::SystemInfo(const char* const compName) : SystemInfoComponentBase(compName) {}

SystemInfo::~SystemInfo() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void SystemInfo::EMIT_SYSTEM_INFO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const ResetReason resetReason = SystemInfoHardware::SystemInfoHal::getResetReason();

    this->log_ACTIVITY_HI_SystemInfo(resetReason, SystemInfoVersion::PROJECT_COMMIT, SystemInfoVersion::FPRIME_COMMIT,
                                     SystemInfoVersion::SAMD_COMMIT);

    this->tlmWrite_ResetCause(resetReason);
    this->tlmWrite_ProjectCommit(SystemInfoVersion::PROJECT_COMMIT);
    this->tlmWrite_FprimeCommit(SystemInfoVersion::FPRIME_COMMIT);
    this->tlmWrite_SamdCommit(SystemInfoVersion::SAMD_COMMIT);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Samd21

// ======================================================================
// \title  SamdHealth.cpp
// \author tumbar
// \brief  cpp file for SamdHealth component implementation class
// ======================================================================

#include "fprime-samd/Drv/SamdHealth/SamdHealth.hpp"
#include "fprime-samd/Drv/SamdHealth/SamdHealthHardware.hpp"
#include "fprime-samd/Drv/SamdHealth/SamdHealthVersion.hpp"
#include "fprime-samd/Drv/Types/ResetReasonEnumAc.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

SamdHealth::SamdHealth(const char* const compName) : SamdHealthComponentBase(compName) {}

SamdHealth::~SamdHealth() {}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void SamdHealth::EMIT_SYSTEM_INFO_cmdHandler(FwOpcodeType opCode, U32 cmdSeq) {
    const ResetReason resetReason = SamdHealthHardware::SamdHealthHal::getResetReason();

    this->log_ACTIVITY_HI_SystemInfo(resetReason, SamdHealthVersion::PROJECT_COMMIT, SamdHealthVersion::FPRIME_COMMIT,
                                     SamdHealthVersion::SAMD_COMMIT);

    // Reported field by field: a single struct-valued channel would not fit
    // FW_TLM_BUFFER_MAX_SIZE
    this->tlmWrite_ResetCause(resetReason);
    this->tlmWrite_ProjectCommit(SamdHealthVersion::PROJECT_COMMIT);
    this->tlmWrite_FprimeCommit(SamdHealthVersion::FPRIME_COMMIT);
    this->tlmWrite_SamdCommit(SamdHealthVersion::SAMD_COMMIT);

    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

}  // namespace Samd21

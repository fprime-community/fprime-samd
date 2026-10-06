// ======================================================================
// \title  SystemInfoHardware.cpp
// \author tumbar
// \brief  Hardware implementation for SAMD21 health and state queries
//
// This file is compiled for SAMD21 target builds only. For Linux/test builds,
// SystemInfoHardwareStub.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/SystemInfo/SystemInfoHardware.hpp"
#include "samd.h"

namespace Samd21 {
namespace SystemInfoHardware {

ResetReason SystemInfoHal::getResetReason() {
    // PM RCAUSE, datasheet 17.8.14. Several flags may be set simultaneously, so the order
    // of these checks defines which cause is reported; see SystemInfoHal::getResetReason.
    const U8 rcause = REG_PM_RCAUSE;

    if (rcause & PM_RCAUSE_POR) {
        return ResetReason::POWER_ON;
    }

    if (rcause & PM_RCAUSE_BOD12) {
        return ResetReason::BROWN_OUT_12;
    }

    if (rcause & PM_RCAUSE_BOD33) {
        return ResetReason::BROWN_OUT_33;
    }

    if (rcause & PM_RCAUSE_EXT) {
        return ResetReason::EXTERNAL;
    }

    if (rcause & PM_RCAUSE_WDT) {
        return ResetReason::WATCHDOG_TIMER;
    }

    if (rcause & PM_RCAUSE_SYST) {
        return ResetReason::SYSTEM;
    }

    return ResetReason::UNKNOWN;
}

}  // namespace SystemInfoHardware
}  // namespace Samd21

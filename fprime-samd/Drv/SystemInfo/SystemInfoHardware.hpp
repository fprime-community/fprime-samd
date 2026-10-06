// ======================================================================
// \title  SystemInfoHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for SAMD21 health and state queries
// ======================================================================

#ifndef Samd21_SystemInfoHardware_HPP
#define Samd21_SystemInfoHardware_HPP

#include "fprime-samd/Drv/Types/ResetReasonEnumAc.hpp"

namespace Samd21 {
namespace SystemInfoHardware {

//! Hardware abstraction layer for MCU health and state queries
//! This allows unit testing by providing stub implementations on non-MCU platforms
struct SystemInfoHal {
    //! Read the cause of the most recent reset from the power manager
    //!
    //! More than one RCAUSE flag can be set at once, so the register is decoded in
    //! priority order: POWER_ON, BROWN_OUT_12, BROWN_OUT_33, EXTERNAL, WATCHDOG_TIMER,
    //! SYSTEM. UNKNOWN is returned when no known flag is set.
    //! \return Reason for the most recent reset
    static ResetReason getResetReason();
};

//! Test helper functions (only available in stub implementation)
#ifndef __SAMD21__

//! Observable state recorded by the stub HAL for unit testing
struct SystemInfoState {
    //! Number of times getResetReason() was called
    U32 get_reset_reason_count;

    //! Value returned by the next getResetReason() call
    ResetReason reset_reason;
};

//! Get the global stub health state
//! \return Reference to global health state
SystemInfoState& getSystemInfoState();

//! Reset stub health state for clean test runs
void resetSystemInfoState();

//! Set the value that getResetReason() will return
//! \param reason Reset reason to report on the next getResetReason()
void setResetReason(ResetReason reason);

#endif

}  // namespace SystemInfoHardware
}  // namespace Samd21

#endif

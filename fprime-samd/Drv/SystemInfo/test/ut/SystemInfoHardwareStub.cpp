// ======================================================================
// \title  SystemInfoHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for SAMD21 health queries (Linux/test builds)
// ======================================================================

#include "fprime-samd/Drv/SystemInfo/SystemInfoHardware.hpp"

namespace Samd21 {
namespace SystemInfoHardware {

//! Reset reason reported until a test injects another one. POWER_ON mirrors the hardware
//! RCAUSE reset value of 0x01 (RCAUSE.POR set), see datasheet 17.8.14.
static constexpr ResetReason::T DEFAULT_RESET_REASON = ResetReason::POWER_ON;

//! Global health state instance for stub
static SystemInfoState g_health_state = {
    .get_reset_reason_count = 0,
    .reset_reason = DEFAULT_RESET_REASON,
};

SystemInfoState& getSystemInfoState() {
    return g_health_state;
}

ResetReason SystemInfoHal::getResetReason() {
    g_health_state.get_reset_reason_count++;
    return g_health_state.reset_reason;
}

//! Test helper: reset stub state for clean test runs
void resetSystemInfoState() {
    g_health_state.get_reset_reason_count = 0;
    g_health_state.reset_reason = DEFAULT_RESET_REASON;
}

//! Test helper: set the reason reported by the next getResetReason()
void setResetReason(ResetReason reason) {
    g_health_state.reset_reason = reason;
}

}  // namespace SystemInfoHardware
}  // namespace Samd21

// ======================================================================
// \title  FatalHandlerHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for SAMD21 fatal handling (Linux/test builds)
// ======================================================================

#include <cstdio>
#include "fprime-samd/Svc/FatalHandler/FatalHandlerHardware.hpp"

namespace Samd21 {
namespace FatalHandlerHardware {

//! Global fatal handler state instance for stub
static FatalHandlerState g_fatal_state = {
    .disable_interrupts_count = 0,
    .delay_count = 0,
    .delay_total_ms = 0,
    .reset_count = 0,
};

FatalHandlerState& getFatalHandlerState() {
    return g_fatal_state;
}

void FatalHandlerHal::disableInterrupts() {
    g_fatal_state.disable_interrupts_count++;
}

void FatalHandlerHal::delayMs(U32 ms) {
    g_fatal_state.delay_count++;
    g_fatal_state.delay_total_ms += ms;
}

void FatalHandlerHal::reset() {
    g_fatal_state.reset_count++;
    // There is no MCU to reset. Leave a marker so death tests can tell that an
    // assert was routed through the FatalHandler; the framework aborts afterwards
    // (FW_ASSERTIONS_ALWAYS_ABORT).
    (void)std::fputs("FatalHandlerHal::reset\n", stderr);
}

//! Test helper: reset stub state for clean test runs
void resetFatalHandlerState() {
    g_fatal_state.disable_interrupts_count = 0;
    g_fatal_state.delay_count = 0;
    g_fatal_state.delay_total_ms = 0;
    g_fatal_state.reset_count = 0;
}

}  // namespace FatalHandlerHardware
}  // namespace Samd21

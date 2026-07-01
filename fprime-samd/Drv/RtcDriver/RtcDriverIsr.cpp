#include "fprime-samd/Drv/RtcDriver/RtcDriverHardware.hpp"

//! RTC interrupt handler
extern "C" __attribute__((used)) void RTC_Handler(void) {
    Samd21::RtcHardware::RtcState& state = Samd21::RtcHardware::getRtcState();

    if (!state.deadline_reached) {
        // We did not set this deadline reached before the next RTC cycle
        // We should indicate slip in the cycle
        state.deadline_exceeded++;
    }

    state.deadline_reached = false;
    state.wakeup_interrupt = true;
    state.n_ticks++;

    Samd21::RtcHardware::RtcHal::clearRtcInterrupt();
}

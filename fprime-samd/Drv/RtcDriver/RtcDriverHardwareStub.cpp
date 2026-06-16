// ======================================================================
// \title  RtcDriverHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for RTC peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, RtcDriverHardware.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/RtcDriver/RtcDriverHardware.hpp"

namespace Samd21 {
namespace RtcHardware {

//! Global RTC state instance for stub
static RtcState g_rtc_state = {
    .wakeup_interrupt = false,
    .deadline_reached = false,
    .deadline_exceeded = 0,
    .configured = false,
    .n_ticks = 0,
    .rate_hz = RtcDriver::TickRate::TICK_1_HZ,
    .clock_rate_hz = 0,
};

//! Simulated RTC counter for stub
static U32 g_rtc_counter = 0;

RtcState& getRtcState() {
    return g_rtc_state;
}

void RtcHal::waitForGclk() {
    // Stub: no-op
}

void RtcHal::waitForRtcMode0() {
    // Stub: no-op
}

void RtcHal::waitForInterrupt() {
    // Stub: no-op for tests
    // Tests should call simulateRtcInterrupt() explicitly before cycle()
}

void RtcHal::configureGclk(RtcDriver::ClockSource clock_source) {
    // Stub: no-op for tests
}

void RtcHal::configureRtc(U32 clock_rate_hz, RtcDriver::TickRate rate) {
    // Stub: reset counter
    g_rtc_counter = 0;
}

void RtcHal::enableRtc() {
    // Stub: no-op
}

U32 RtcHal::readRtcCounter() {
    // Stub: return simulated counter
    return g_rtc_counter;
}

void RtcHal::clearRtcInterrupt() {
    // Stub: no-op
}

//! Test helper: simulate an RTC interrupt
void simulateRtcInterrupt() {
    RtcState& state = getRtcState();

    if (!state.deadline_reached) {
        state.deadline_exceeded++;
    }

    state.deadline_reached = false;
    state.wakeup_interrupt = true;
    state.n_ticks++;
}

//! Test helper: reset RTC state for clean test runs
void resetRtcState() {
    g_rtc_state.wakeup_interrupt = false;
    g_rtc_state.deadline_reached = false;
    g_rtc_state.deadline_exceeded = 0;
    g_rtc_state.configured = false;
    g_rtc_state.n_ticks = 0;
    g_rtc_state.rate_hz = RtcDriver::TickRate::TICK_1_HZ;
    g_rtc_state.clock_rate_hz = 0;
    g_rtc_counter = 0;
}

//! Test helper: set counter value for time queries
void setRtcCounter(U32 counter) {
    g_rtc_counter = counter;
}

}  // namespace RtcHardware
}  // namespace Samd21

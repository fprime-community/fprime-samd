// ======================================================================
// \title  RtcDriver.cpp
// \author tumbar
// \brief  cpp file for RtcDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/RtcDriver/RtcDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "Os/RawTime.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "config/RtcDriverConfig.hpp"
#include "fprime-samd/Drv/RtcDriver/RawTime.hpp"
#include "fprime-samd/Drv/RtcDriver/RtcDriverHardware.hpp"

namespace Os {
namespace Samd21 {

Os::Samd21::Samd21RawTimeHandle timeNow() {
    Os::Samd21::Samd21RawTimeHandle time;

    ::Samd21::RtcHardware::RtcState& state = ::Samd21::RtcHardware::getRtcState();

    if (state.configured) {
        U32 seconds_offset = state.n_ticks / static_cast<U32>(state.rate_hz);
        U32 subsecond_ticks = state.n_ticks % static_cast<U32>(state.rate_hz);

        constexpr U32 USEC_PER_SEC = 1000000;
        U32 subtick_counter = ::Samd21::RtcHardware::RtcHal::readRtcCounter();

        // Use 64-bit arithmetic to avoid overflow, then cast result back to U32
        // Maximum intermediate value: 32768 * 1000000 = 32,768,000,000 (fits in U64)
        U32 usecond_offset =
            static_cast<U32>((static_cast<U64>(subsecond_ticks) * USEC_PER_SEC) / static_cast<U32>(state.rate_hz));
        U32 useconds_counter =
            static_cast<U32>((static_cast<U64>(subtick_counter) * USEC_PER_SEC) / state.clock_rate_hz);

        // Compute the final time
        U32 useconds = usecond_offset + useconds_counter;
        U32 seconds = seconds_offset + (useconds / USEC_PER_SEC);
        useconds %= USEC_PER_SEC;

        time.m_seconds = seconds;
        time.m_micros = useconds;
    } else {
        // RTC is not configured, we have no way of determining the processor time
        time.m_seconds = 0;
        time.m_micros = 0;
    }

    return time;
}

}  // namespace Samd21
}  // namespace Os

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

RtcDriver ::RtcDriver(const char* const compName) : RtcDriverComponentBase(compName) {}

void RtcDriver ::configure(ClockSource clk_source, TickRate rate) {
    RtcHardware::RtcState& state = RtcHardware::getRtcState();
    FW_ASSERT(!state.configured);

    // Validate the rate is one of our predefined rates
    switch (rate) {
        case TickRate::TICK_1_HZ:
        case TickRate::TICK_2_HZ:
        case TickRate::TICK_4_HZ:
        case TickRate::TICK_8_HZ:
        case TickRate::TICK_16_HZ:
        case TickRate::TICK_32_HZ:
        case TickRate::TICK_64_HZ:
        case TickRate::TICK_128_HZ:
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(rate));
            break;
    }

    // Select the clock source
    switch (clk_source) {
        case RtcDriver::ClockSource::InternalOscillator:
            state.clock_rate_hz = RtcDriver::INTERNAL_CLOCK_FREQ_HZ;
            break;
        case RtcDriver::ClockSource::ExternalOscillator:
            state.clock_rate_hz = ::Samd21::RtcDriverConfig::EXTERNAL_CLOCK_FREQ_HZ;
            break;
        case RtcDriver::ClockSource::UltraLowPowerOscillator:
            state.clock_rate_hz = RtcDriver::INTERNAL_CLOCK_FREQ_HZ;
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(clk_source));
            break;
    }

    // Configure GCLK and get the clock rate
    RtcHardware::RtcHal::configureGclk(clk_source);

    // Configure RTC peripheral
    RtcHardware::RtcHal::configureRtc(state.clock_rate_hz, rate);

    // Initialize state
    state.n_ticks = 0;
    state.rate_hz = rate;
    state.configured = true;
}

void RtcDriver ::enable() {
    RtcHardware::RtcState& state = RtcHardware::getRtcState();
    FW_ASSERT(state.configured);

    // Enable the RTC peripheral
    RtcHardware::RtcHal::enableRtc();
}

void RtcDriver ::activeIn_handler(FwIndexType portNum, U32 context) {
    // Acknowledge the ISR on every cycle
    RtcHardware::RtcState& state = RtcHardware::getRtcState();
    state.deadline_reached = true;

    if (state.wakeup_interrupt) {
        // Acknowledge the interrupt
        state.wakeup_interrupt = false;

        // The RTC woke us up, emit the signal
        Os::RawTime cycle_start;
        auto status = cycle_start.now();
        FW_ASSERT(status == Os::RawTimeInterface::Status::OP_OK, status);

        this->tlmWrite_CycleOverrun(state.deadline_exceeded);
        this->CycleOut_out(0, cycle_start);
    }
}

RtcDriver ::~RtcDriver() {}

}  // namespace Samd21

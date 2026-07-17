// ======================================================================
// \title  RtcDriverHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for RTC driver peripheral operations
// ======================================================================

#ifndef Samd21_RtcDriverHardware_HPP
#define Samd21_RtcDriverHardware_HPP

#include "fprime-samd/Drv/RtcDriver/RtcDriver.hpp"

namespace Samd21 {
namespace RtcHardware {

//! Hardware abstraction layer for RTC peripheral operations
//! This allows unit testing by providing stub implementations on non-MCU platforms
struct RtcHal {
    //! Wait for GCLK synchronization
    static void waitForGclk();

    //! Wait for RTC MODE0 synchronization
    static void waitForRtcMode0();

    //! Configure GCLK4 for RTC
    //! \param clock_source Clock source selection
    static void configureGclk(RtcDriver::ClockSource clock_source);

    //! Configure RTC peripheral in MODE0 (32-bit counter)
    //! \param clock_rate_hz Input clock rate
    //! \param rate Tick rate for compare interrupt
    static void configureRtc(U32 clock_rate_hz, RtcDriver::TickRate rate);

    //! Enable RTC peripheral and interrupts
    static void enableRtc();

    //! Read current RTC counter value
    //! \return 32-bit counter value
    static U32 readRtcCounter();

    //! Clear RTC interrupt flag
    static void clearRtcInterrupt();
};

//! Global state tracking (shared between ISR and driver)
struct RtcState {
    //! Wakeup from deepsleep came from RTC
    volatile bool wakeup_interrupt;

    //! Deadline reached flag (acts like a watchdog)
    volatile bool deadline_reached;

    //! Count of deadline exceeded events
    volatile U16 deadline_exceeded;

    //! Flag indicating RTC driver has been configured
    bool configured;

    //! Monotonic tick counter
    volatile U32 n_ticks;

    //! Configured tick rate
    RtcDriver::TickRate rate_hz;

    //! Configured clock rate
    U32 clock_rate_hz;
};

//! Get the global RTC state
//! \return Reference to global RTC state
RtcState& getRtcState();

//! Test helper functions (only available in stub implementation)
#ifndef __SAMD21__
//! Simulate an RTC interrupt
void simulateRtcInterrupt();

//! Reset RTC state for clean test runs
void resetRtcState();

//! Set counter value for time queries
void setRtcCounter(U32 counter);
#endif

}  // namespace RtcHardware
}  // namespace Samd21

#endif

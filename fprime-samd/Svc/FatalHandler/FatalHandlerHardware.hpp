// ======================================================================
// \title  FatalHandlerHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for SAMD21 fatal handling
// ======================================================================

#ifndef Samd21_FatalHandlerHardware_HPP
#define Samd21_FatalHandlerHardware_HPP

#include "Fw/FPrimeBasicTypes.hpp"

namespace Samd21 {
namespace FatalHandlerHardware {

//! Hardware abstraction layer for the fatal handler
//! This allows native builds by providing stub implementations on non-MCU platforms
struct FatalHandlerHal {
    //! Disable all interrupts
    static void disableInterrupts();

    //! Busy-wait for the given number of milliseconds
    static void delayMs(U32 ms);

    //! Reset the MCU
    static void reset();
};

//! Test helper functions (only available in stub implementation)
#ifndef __SAMD21__

//! Observable state recorded by the stub HAL for unit testing
struct FatalHandlerState {
    //! Number of times disableInterrupts() was called
    U32 disable_interrupts_count;

    //! Number of times delayMs() was called
    U32 delay_count;

    //! Sum of every delayMs() argument
    U32 delay_total_ms;

    //! Number of times reset() was called
    U32 reset_count;
};

//! Get the global stub fatal handler state
//! \return Reference to global fatal handler state
FatalHandlerState& getFatalHandlerState();

//! Reset stub fatal handler state for clean test runs
void resetFatalHandlerState();

#endif

}  // namespace FatalHandlerHardware
}  // namespace Samd21

#endif

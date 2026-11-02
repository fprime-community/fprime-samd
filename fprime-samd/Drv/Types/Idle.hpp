// ======================================================================
// \title  Idle.hpp
// \brief  Sleep-until-interrupt for the SAMD21 main context, race-free against the ISRs
// ======================================================================

#ifndef Samd21_Idle_HPP
#define Samd21_Idle_HPP

#include "Fw/FPrimeBasicTypes.hpp"

namespace Samd21 {

//! Main-context idle for a deployment driven by Svc::PassiveCycler.
//!
//! Driver ISRs only latch work; the components that act on it do so from their activeIn
//! port, called by the cycler. An interrupt that lands after its component's turn in a pass
//! but before the WFI has already been taken when the MCU sleeps, so nothing wakes it until
//! the next unrelated interrupt. Every interrupt entry sets samd21_irq_taken (Mcu/reset.c);
//! arm() clears it before the pass and sleepUnlessInterrupted() goes to sleep only if it is
//! still clear, with interrupts masked between the check and the WFI (a pending interrupt
//! ends the WFI even with PRIMASK set and is serviced once interrupts are re-enabled).
//!
//! Preconditions: main context, interrupts enabled, no Samd21::CriticalSection open.
class Idle {
  public:
    //! Forget the interrupts taken so far; call before servicing the consumers
    static void arm();

    //! WFI unless an interrupt was taken since arm(); returns with interrupts enabled
    static void sleepUnlessInterrupted();
};

//! Test hooks of the native stub (IdleStub.cpp); not present in the SAMD21 build
namespace IdleStub {
struct State {
    U32 arms;          //!< arm() calls
    U32 sleeps;        //!< sleepUnlessInterrupted() calls that slept
    U32 skips;         //!< sleepUnlessInterrupted() calls that did not sleep
    bool interrupted;  //!< the simulated samd21_irq_taken flag; set it to simulate an ISR
};
State& getState();
void resetState();
}  // namespace IdleStub

}  // namespace Samd21

#endif

// ======================================================================
// \title  CriticalSection.cpp
// \author tumbar
// \brief  cpp file for CriticalSection utility
// ======================================================================

#include "fprime-samd/Drv/Types/CriticalSection.hpp"
#include "Fw/FPrimeBasicTypes.hpp"
#include "sam.h"

namespace Samd21 {

//! Depth of the currently-held critical section nesting
static volatile U32 s_critical_section_counter = 0;

//! Whether interrupts were enabled when the outermost section was entered
static volatile U32 s_prev_interrupt_state = 0;

void CriticalSection::enter() {
    if (!s_critical_section_counter) {
        if (__get_PRIMASK() == 0) {  // IRQ enabled?
            __disable_irq();         // Disable it
            __DMB();
            s_prev_interrupt_state = 1;
        } else {
            // Make sure to save the prev state as false
            s_prev_interrupt_state = 0;
        }
    }

    s_critical_section_counter++;
}

void CriticalSection::leave() {
    // Check if the user is trying to leave a critical section
    // when not in a critical section
    if (s_critical_section_counter > 0) {
        s_critical_section_counter--;

        // Only enable global interrupts when the counter
        // reaches 0 and the state of the global interrupt flag
        // was enabled when entering critical state
        if ((!s_critical_section_counter) && s_prev_interrupt_state) {
            __DMB();
            __enable_irq();
        }
    }
}

CriticalSection::CriticalSection() {
    CriticalSection::enter();
}

CriticalSection::~CriticalSection() {
    CriticalSection::leave();
}

}  // namespace Samd21

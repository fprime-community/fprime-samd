// ======================================================================
// \title  Delay.hpp
// \author Andrei Tumbar
// \brief  Bare-metal delay implementation for SAMD21
// ======================================================================
#ifndef FPRIME_SAMD_MCU_DELAY_HPP
#define FPRIME_SAMD_MCU_DELAY_HPP

#include <sam.h>

// Simple busy-wait delay in milliseconds
// Assumes 48MHz clock (adjust CYCLES_PER_MS if needed)
inline void delay(unsigned long ms) {
    // At 48MHz, approximately 48000 cycles per millisecond
    // Accounting for loop overhead, use a conservative estimate
    constexpr unsigned long CYCLES_PER_MS = 12000;  // Tuned for accuracy

    volatile unsigned long count;
    for (unsigned long i = 0; i < ms; i++) {
        for (count = 0; count < CYCLES_PER_MS; count++) {
            __NOP();  // No operation - prevents optimizer from removing loop
        }
    }
}

#endif  // FPRIME_SAMD_MCU_DELAY_HPP

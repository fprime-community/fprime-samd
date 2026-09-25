// ======================================================================
// \title  GpioDriverIsr.cpp
// \author tumbar
// \brief  EIC (External Interrupt Controller) interrupt handler
//
// This file is only compiled for SAMD21 target builds. A single EIC_Handler
// services every EXTINT line; it dispatches each pending line's edge to the
// GpioDriver instance registered for that line via registerInterruptHandler().
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/GpioDriverHardware.hpp"

//! Number of EIC external interrupt lines (EXTINT[0..15]).
static constexpr U8 EXTINT_LINE_COUNT = 16;

//! EIC interrupt handler
extern "C" __attribute__((used)) void EIC_Handler(void) {
    using namespace Samd21;

    // Snapshot the pending lines, then dispatch each to its registered component.
    const U32 flags = GpioHardware::GpioHal::getInterruptFlags();

    for (U8 line = 0; line < EXTINT_LINE_COUNT; line++) {
        if ((flags & (static_cast<U32>(1) << line)) == 0) {
            continue;
        }

        GpioDriver* handler = GpioHardware::getInterruptHandler(line);
        if (handler != nullptr) {
            handler->gpioInterruptIsr();
        }
    }

    // Acknowledge every serviced line so the EIC can detect the next edge.
    GpioHardware::GpioHal::clearInterruptFlags(flags);
}

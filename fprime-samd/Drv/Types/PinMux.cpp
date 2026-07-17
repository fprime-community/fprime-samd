// ======================================================================
// \title  PinMux.cpp
// \author tumbar
// \brief  cpp file for PinMux utility
// ======================================================================

#include "fprime-samd/Drv/Types/PinMux.hpp"
#include "samd.h"

namespace Samd21 {
void PinMux ::configure(U32 pinmux) {
    // 1. Extract the pin number and multiplexer function
    uint16_t pin = (pinmux >> 16);          // Upper 16 bits contain the pin index (e.g., 8)
    uint16_t mux_func = (pinmux & 0xFFFF);  // Lower 16 bits contain the mux letter (e.g., 2 for 'C')

    // 2. Identify the PORT group (Group 0 = PORTA, Group 1 = PORTB)
    uint8_t port_group = pin / 32;
    uint8_t pin_index = pin % 32;

    // 3. Enable the Peripheral Multiplexer for this specific pin
    PORT->Group[port_group].PINCFG[pin_index].reg |= PORT_PINCFG_PMUXEN;

    // 4. Update the PMUX register.
    // Pins share a PMUX register: Even index uses PMUXE, Odd index uses PMUXO
    if (pin_index % 2 == 0) {
        // Clear old even mux and set new one
        PORT->Group[port_group].PMUX[pin_index >> 1].bit.PMUXE = mux_func;
    } else {
        // Clear old odd mux and set new one
        PORT->Group[port_group].PMUX[pin_index >> 1].bit.PMUXO = mux_func;
    }
}

}  // namespace Samd21

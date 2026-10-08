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

void PinMux ::configureGpioInput(U32 pin_id, Gpio::InputPullMode pull_mode) {
    const U8 port_group = pin_id / 32;
    const U8 pin_index = pin_id % 32;
    const U32 pin_mask = static_cast<U32>(1) << pin_index;
    PortGroup& group = PORT->Group[port_group];
    U8 pin_cfg = static_cast<U8>(PORT_PINCFG_INEN);

    // Overwrite (not |=) also clears PMUXEN, reclaiming a pin that was
    // previously routed to a SERCOM/peripheral function.
    group.DIRCLR.reg = pin_mask;

    switch (pull_mode) {
        case Gpio::InputPullMode::NO_PULL:
            break;
        case Gpio::InputPullMode::PULL_DOWN:
            group.OUTCLR.reg = pin_mask;
            pin_cfg |= static_cast<U8>(PORT_PINCFG_PULLEN);
            break;
        case Gpio::InputPullMode::PULL_UP:
            group.OUTSET.reg = pin_mask;
            pin_cfg |= static_cast<U8>(PORT_PINCFG_PULLEN);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(pull_mode));
            break;
    }

    group.PINCFG[pin_index].reg = pin_cfg;
}

void PinMux ::configureGpioOutput(U32 pin_id, Fw::Logic initial_state) {
    const U8 port_group = pin_id / 32;
    const U8 pin_index = pin_id % 32;
    const U32 pin_mask = static_cast<U32>(1) << pin_index;
    PortGroup& group = PORT->Group[port_group];

    // Overwrite (not |=) also clears PMUXEN, reclaiming a pin that was
    // previously routed to a SERCOM/peripheral function.
    group.PINCFG[pin_index].reg = static_cast<U8>(PORT_PINCFG_INEN);
    if (initial_state == Fw::Logic::HIGH) {
        group.OUTSET.reg = pin_mask;
    } else {
        group.OUTCLR.reg = pin_mask;
    }
    group.DIRSET.reg = pin_mask;
}

}  // namespace Samd21

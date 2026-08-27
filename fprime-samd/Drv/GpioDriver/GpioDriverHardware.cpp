// ======================================================================
// \title  GpioDriverHardware.cpp
// \author tumbar
// \brief  MCU-specific hardware implementation for PORT (GPIO) peripheral
//
// This file is only compiled for SAMD21 target builds.
// For Linux/test builds, GpioDriverHardwareStub.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/GpioDriverHardware.hpp"
#include "Fw/Types/LogicEnumAc.hpp"
#include "samd.h"

namespace Samd21 {
namespace GpioHardware {

namespace {

//! GCLK generic clock ID that feeds the EIC (see datasheet §21, Table "GCLK_EIC" = 0x05).
//! Edge detection requires GCLK_EIC to be running.
constexpr U8 GCLK_ID_FOR_EIC = 0x05;

//! Number of EIC external interrupt lines (EXTINT[0..15]); a pin maps to line pinIdx % this.
constexpr U8 EXTINT_LINE_COUNT = 16;

//! Peripheral multiplexer function that routes a PORT pin to its EIC EXTINT line.
//! EXTINT is always peripheral function A (mux value 0) on the SAMD21.
constexpr U8 EXTINT_PMUX_FUNCTION = 0;

//! Components registered for ISR dispatch, indexed by EXTINT line. The single
//! EIC_Handler uses this to route each line's edge to the owning component.
GpioDriver* g_interrupt_handlers[EXTINT_LINE_COUNT] = {};

//! Spin until the GCLK finishes synchronizing a clock-domain write.
void waitForGclkSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

//! Spin until the EIC finishes synchronizing an enable/reset write.
void waitForEicSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && EIC->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

//! Translate the driver's interrupt mode to an EIC CONFIG.SENSE field value.
U8 senseForMode(GpioDriver::ExternalInterruptMode interrupt_mode) {
    switch (interrupt_mode) {
        case GpioDriver::ExternalInterruptMode::RISING:
            return static_cast<U8>(EIC_CONFIG_SENSE0_RISE_Val);
        case GpioDriver::ExternalInterruptMode::FALLING:
            return static_cast<U8>(EIC_CONFIG_SENSE0_FALL_Val);
        case GpioDriver::ExternalInterruptMode::BOTH:
            return static_cast<U8>(EIC_CONFIG_SENSE0_BOTH_Val);
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(interrupt_mode));
            return static_cast<U8>(EIC_CONFIG_SENSE0_NONE_Val);
    }
}

}  // namespace

//! Configure the External Interrupt Controller for a single input pin.
//!
//! Follows the initialization order in datasheet §21.6.2.1:
//!   1. Enable CLK_EIC_APB
//!   2. Enable GCLK_EIC (required for edge detection)
//!   3. Write the EIC configuration registers (CONFIGn)
//!   4. Enable the EIC
//!
//! The EIC must be disabled while CONFIGn is written (it is enable-protected), so the
//! sequence is disable -> read-modify-write CONFIGn -> re-enable. Read-modify-write
//! preserves the SENSE fields of any other lines already configured by other instances.
void GpioHal::configureExternalInterrupt(U8 groupIdx, U8 pinIdx, GpioDriver::ExternalInterruptMode interrupt_mode) {
    PortGroup& portGroup = PORT->Group[groupIdx];

    // Route the pin to its EIC EXTINT line via peripheral function A.
    portGroup.PINCFG[pinIdx].reg |= static_cast<U8>(PORT_PINCFG_PMUXEN);
    if ((pinIdx & 1) == 0) {
        portGroup.PMUX[pinIdx >> 1].bit.PMUXE = EXTINT_PMUX_FUNCTION;
    } else {
        portGroup.PMUX[pinIdx >> 1].bit.PMUXO = EXTINT_PMUX_FUNCTION;
    }

    // 1. Enable the EIC APB clock.
    PM->APBAMASK.bit.EIC_ = 1;

    // 2. Enable GCLK_EIC, sourced from Generic Clock Generator 0 (48MHz main clock).
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCLK_ID_FOR_EIC) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
    waitForGclkSync();

    // The EXTINT line, the CONFIG register holding it, and the SENSE bit offset within.
    const U8 extint = pinIdx % EXTINT_LINE_COUNT;
    const U8 configIdx = extint / 8;
    const U8 senseShift = static_cast<U8>((extint % 8) * 4);
    const U32 senseMask = static_cast<U32>(EIC_CONFIG_SENSE0_Msk) << senseShift;
    const U32 lineMask = static_cast<U32>(1) << extint;

    // 3. Disable the EIC so its enable-protected CONFIGn registers can be written.
    EIC->CTRL.bit.ENABLE = 0;
    waitForEicSync();

    // Read-modify-write only this line's SENSE field, leaving other lines untouched.
    U32 config = EIC->CONFIG[configIdx].reg;
    config &= ~senseMask;
    config |= static_cast<U32>(senseForMode(interrupt_mode)) << senseShift;
    EIC->CONFIG[configIdx].reg = config;

    // Clear any stale flag, then enable the interrupt request for this line.
    EIC->INTFLAG.reg = lineMask;
    EIC->INTENSET.reg = lineMask;

    // 4. Re-enable the EIC.
    EIC->CTRL.bit.ENABLE = 1;
    waitForEicSync();

    // Route the EIC interrupt to the NVIC.
    NVIC_EnableIRQ(EIC_IRQn);
}

void GpioHal::configureInput(U8 groupIdx, U8 pinIdx, GpioDriver::InputPullMode input_pull_mode) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    PortGroup& portGroup = PORT->Group[groupIdx];
    U8 pinCfg = static_cast<U8>(PORT_PINCFG_INEN);

    // Configure as an input.
    portGroup.DIRCLR.reg = pinMask;

    switch (input_pull_mode) {
        case GpioDriver::InputPullMode::NO_PULL:
            // No pull up/down resistors are connected
            break;
            // With PULLEN set, the OUT register bit selects the pull direction:
            // OUT=1 -> pull-up, OUT=0 -> pull-down. Set the direction before
            // enabling the pull so the pad never briefly pulls the wrong way.
        case GpioDriver::InputPullMode::PULL_DOWN:
            portGroup.OUTCLR.reg = pinMask;
            pinCfg |= static_cast<U8>(PORT_PINCFG_PULLEN);
            break;
        case GpioDriver::InputPullMode::PULL_UP:
            portGroup.OUTSET.reg = pinMask;
            pinCfg |= static_cast<U8>(PORT_PINCFG_PULLEN);
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(input_pull_mode));
            break;
    }

    portGroup.PINCFG[pinIdx].reg = pinCfg;
}

void GpioHal::configureOutput(U8 groupIdx, U8 pinIdx) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    PortGroup& portGroup = PORT->Group[groupIdx];
    U8 pinCfg = static_cast<U8>(PORT_PINCFG_INEN);

    portGroup.PINCFG[pinIdx].reg = pinCfg;
    portGroup.OUTCLR.reg = pinMask;
    portGroup.DIRSET.reg = pinMask;
}

Fw::Logic GpioHal::read(U8 groupIdx, U8 pinIdx) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    const PortGroup& portGroup = PORT->Group[groupIdx];
    const bool isHigh = (portGroup.IN.reg & pinMask) != 0;

    return isHigh ? Fw::Logic::HIGH : Fw::Logic::LOW;
}

void GpioHal::write(U8 groupIdx, U8 pinIdx, const Fw::Logic& state) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    PortGroup& portGroup = PORT->Group[groupIdx];
    if (state == Fw::Logic::HIGH) {
        portGroup.OUTSET.reg = pinMask;
    } else {
        portGroup.OUTCLR.reg = pinMask;
    }
}

U32 GpioHal::getInterruptFlags() {
    return EIC->INTFLAG.reg;
}

void GpioHal::clearInterruptFlags(U32 mask) {
    // INTFLAG bits are write-1-to-clear.
    EIC->INTFLAG.reg = mask;
}

void registerInterruptHandler(U8 pinIdx, GpioDriver* handler) {
    g_interrupt_handlers[pinIdx % EXTINT_LINE_COUNT] = handler;
}

GpioDriver* getInterruptHandler(U8 extintLine) {
    FW_ASSERT(extintLine < EXTINT_LINE_COUNT, extintLine);
    return g_interrupt_handlers[extintLine];
}

}  // namespace GpioHardware
}  // namespace Samd21

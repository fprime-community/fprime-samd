// ======================================================================
// \title  RtcDriverHardware.cpp
// \author tumbar
// \brief  MCU-specific hardware implementation for RTC peripheral
//
// This file is only compiled for SAMD21 target builds.
// For Linux/test builds, RtcDriverHardwareStub.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/RtcDriver/RtcDriverHardware.hpp"
#include "Fw/Types/Assert.hpp"
#include "samd.h"

namespace Samd21 {
namespace RtcHardware {

//! Global RTC state instance
static RtcState g_rtc_state = {
    .wakeup_interrupt = false,
    .deadline_reached = false,
    .deadline_exceeded = 0,
    .configured = false,
    .n_ticks = 0,
    .rate_hz = RtcDriver::TickRate::TICK_1_HZ,
    .clock_rate_hz = 0,
};

RtcState& getRtcState() {
    return g_rtc_state;
}

void RtcHal::waitForGclk() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

void RtcHal::waitForRtcMode0() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && RTC->MODE0.STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

void RtcHal::waitForInterrupt() {
    asm("wfi");
}

void RtcHal::configureGclk(RtcDriver::ClockSource clock_source) {
    // Configure GCLK4 with no division to get 32768 Hz
    REG_GCLK_GENDIV = GCLK_GENDIV_ID(0x4) | GCLK_GENDIV_DIV(1);
    waitForGclk();

    // Select the clock source
    switch (clock_source) {
        case RtcDriver::ClockSource::InternalOscillator:
            REG_GCLK_GENCTRL = GCLK_GENCTRL_ID(0x04) | GCLK_GENCTRL_SRC_OSC32K;
            break;
        case RtcDriver::ClockSource::ExternalOscillator:
            REG_GCLK_GENCTRL = GCLK_GENCTRL_ID(0x04) | GCLK_GENCTRL_SRC_XOSC32K;
            break;
        case RtcDriver::ClockSource::UltraLowPowerOscillator:
            REG_GCLK_GENCTRL = GCLK_GENCTRL_ID(0x04) | GCLK_GENCTRL_SRC_OSCULP32K;
            break;
        default:
            FW_ASSERT(0, static_cast<FwAssertArgType>(clock_source));
            break;
    }

    waitForGclk();

    // Enable the clock generator
    GCLK->GENCTRL.bit.GENEN = 1;
    waitForGclk();

    // route output to the RTC peripheral
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_RTC | GCLK_CLKCTRL_GEN_GCLK4;
    waitForGclk();

    // Enable the clock on RTC
    GCLK->CLKCTRL.bit.CLKEN = 1;
    waitForGclk();

    // Apply power to the RTC
    PM->APBASEL.bit.APBADIV = 0;  // don't prescale the PM clock
    PM->APBAMASK.bit.RTC_ = 1;    // unmask the RTC
}

void RtcHal::configureRtc(U32 clock_rate_hz, RtcDriver::TickRate rate) {
    // Disable the RTC to configure it
    RTC->MODE0.CTRL.bit.ENABLE = 0;
    waitForRtcMode0();

    // Set the RTC to 32-bit counter mode
    // Clear on match to reset the internal counter. RTC only tracks the sub-tick time
    // Do not prescale the incoming clock for maximum accuracy of sub-tick.
    // PRESCALER(0) = DIV1 means no prescaling, CLK_RTC_CNT = GCLK_RTC = 32768 Hz
    REG_RTC_MODE0_CTRL = RTC_MODE0_CTRL_MODE(0) | RTC_MODE0_CTRL_MATCHCLR | RTC_MODE0_CTRL_PRESCALER(0);
    waitForRtcMode0();

    // Clear the RTC counter
    RTC->MODE0.COUNT.reg = 0x0;
    waitForRtcMode0();

    // Count up to the clock frequency over the tick rate
    RTC->MODE0.COMP[0].reg = clock_rate_hz / static_cast<U32>(rate);
    waitForRtcMode0();

    // Enable the compare interrupt
    REG_RTC_MODE0_INTENSET = RTC_MODE0_INTENSET_CMP0;
    waitForRtcMode0();
}

void RtcHal::enableRtc() {
    // Enable the RTC interrupt in the NVIC
    NVIC_ClearPendingIRQ(IRQn::RTC_IRQn);
    NVIC_SetPriority(IRQn::RTC_IRQn, 0);  // Highest priority
    NVIC_EnableIRQ(IRQn::RTC_IRQn);

    // Enable the RTC
    RTC->MODE0.CTRL.bit.ENABLE = 1;
    waitForRtcMode0();
}

U32 RtcHal::readRtcCounter() {
    // Request a read sync for the 32-bit counter in Mode 0
    RTC->MODE0.READREQ.reg = RTC_READREQ_RCONT | RTC_READREQ_ADDR(0);

    // Wait for the peripheral to synchronize
    waitForRtcMode0();

    return RTC->MODE0.COUNT.reg;
}

void RtcHal::clearRtcInterrupt() {
    // Acknowledge the RTC interrupt and clear the flag to receive another one
    REG_RTC_MODE0_INTFLAG |= RTC_MODE0_INTFLAG_CMP0;
}

}  // namespace RtcHardware
}  // namespace Samd21

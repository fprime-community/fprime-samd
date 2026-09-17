// ======================================================================
// \title  AdcDriverHardware.cpp
// \author crsmith
// \brief  Hardware implementation for the ADC peripheral (SAMD21 target builds)
//
// This file is compiled for SAMD21 target builds and contains the actual
// hardware register access. For Linux/test builds, AdcDriverHardwareStub.cpp
// is used instead.
//
// Note: The component (AdcDriver.cpp) maintains port-to-channel mapping state.
// This HAL only handles the hardware register operations.
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/AdcDriverHardware.hpp"
#include "sam.h"

namespace Samd21 {
namespace AdcHardware {

void AdcHal::waitForGclkSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }
    // Check if we timed out
    FW_ASSERT(limit != 0);
}

void AdcHal::waitForAdcSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && ADC->STATUS.bit.SYNCBUSY) {
        limit--;
    }
    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForResultReady() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && !ADC->INTFLAG.bit.RESRDY) {
        limit--;
    }
    // Check if we timed out
    FW_ASSERT(limit != 0);
}

void AdcHal::configure(AdcDriver::VoltageReference ref,
                        AdcDriver::Resolution res,
                        AdcDriver::SampleCount samples,
                        U8 samplingTime,
                        AdcDriver::Gain gain) {
    // Enable the APB clock for the ADC
    PM->APBCMASK.reg |= PM_APBCMASK_ADC;

    // Enable generic clock for ADC (use GCLK0 - 48MHz with DIV32 prescaler = 1.5MHz ADC clock)
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_ADC;
    AdcHal::waitForGclkSync();

    // Load calibration data from NVM
    U32 bias = (*((U32*)ADC_FUSES_BIASCAL_ADDR) & ADC_FUSES_BIASCAL_Msk) >> ADC_FUSES_BIASCAL_Pos;
    U32 linearity = (*((U32*)ADC_FUSES_LINEARITY_0_ADDR) & ADC_FUSES_LINEARITY_0_Msk) >> ADC_FUSES_LINEARITY_0_Pos;
    linearity |= ((*((U32*)ADC_FUSES_LINEARITY_1_ADDR) & ADC_FUSES_LINEARITY_1_Msk) >> ADC_FUSES_LINEARITY_1_Pos) << 5;

    // Software reset to ensure clean state
    ADC->CTRLA.bit.SWRST = 1;
    AdcHal::waitForAdcSync();

    // Write calibration data
    ADC->CALIB.reg = ADC_CALIB_BIAS_CAL(bias) | ADC_CALIB_LINEARITY_CAL(linearity);

    // Configure voltage reference
    U8 refsel = 0;
    switch (ref) {
        case AdcDriver::VoltageReference::INT1V:
            refsel = ADC_REFCTRL_REFSEL_INT1V_Val;
            break;
        case AdcDriver::VoltageReference::INTVCC0:
            refsel = ADC_REFCTRL_REFSEL_INTVCC0_Val;
            break;
        case AdcDriver::VoltageReference::INTVCC1:
            refsel = ADC_REFCTRL_REFSEL_INTVCC1_Val;
            break;
        case AdcDriver::VoltageReference::VREFA:
            refsel = ADC_REFCTRL_REFSEL_AREFA_Val;
            break;
        case AdcDriver::VoltageReference::VREFB:
            refsel = ADC_REFCTRL_REFSEL_AREFB_Val;
            break;
    }
    ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL(refsel) | ADC_REFCTRL_REFCOMP;
    AdcHal::waitForAdcSync();

    // Configure resolution and prescaler
    U8 ressel = 0;
    switch (res) {
        case AdcDriver::Resolution::RES_8BIT:
            ressel = ADC_CTRLB_RESSEL_8BIT_Val;
            break;
        case AdcDriver::Resolution::RES_10BIT:
            ressel = ADC_CTRLB_RESSEL_10BIT_Val;
            break;
        case AdcDriver::Resolution::RES_12BIT:
            ressel = ADC_CTRLB_RESSEL_12BIT_Val;
            break;
    }
    ADC->CTRLB.reg = ADC_CTRLB_RESSEL(ressel) | ADC_CTRLB_PRESCALER_DIV32;
    AdcHal::waitForAdcSync();

    // Configure averaging
    U8 samplenum = static_cast<U8>(samples);
    U8 adjres = samplenum;
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM(samplenum) | ADC_AVGCTRL_ADJRES(adjres);

    // Configure sampling time
    FW_ASSERT(samplingTime <= 63, samplingTime);
    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(samplingTime);

    // Enable ADC first (must be enabled before configuring interrupts)
    ADC->CTRLA.bit.ENABLE = 1;
    AdcHal::waitForAdcSync();

    // Per SAMD21 datasheet: perform dummy conversion after reference change
    U8 dummyGain = static_cast<U8>(gain);
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS_SCALEDIOVCC | ADC_INPUTCTRL_MUXNEG_GND | ADC_INPUTCTRL_GAIN(dummyGain);
    AdcHal::waitForAdcSync();
    ADC->SWTRIG.bit.START = 1;
    waitForResultReady();
    (void)ADC->RESULT.reg;                  // Read and discard
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;  // Clear flag

    // Clear any pending interrupt flags after dummy conversion
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY | ADC_INTFLAG_OVERRUN;
}

void AdcHal::configureChannel(AdcDriver::AdcChannel channel) {
    // Enable internal voltage references for special channels
    // Called only for TEMP and BANDGAP by the component
    // (SCALEDIOVCC and external channels AIN0-AIN19 require no setup)
    if (channel == AdcDriver::AdcChannel::TEMP) {
        SYSCTRL->VREF.reg |= SYSCTRL_VREF_TSEN;  // Enable temperature sensor
    } else if (channel == AdcDriver::AdcChannel::BANDGAP) {
        SYSCTRL->VREF.reg |= SYSCTRL_VREF_BGOUTEN;  // Enable bandgap output
    }
}

void AdcHal::selectChannel(AdcDriver::AdcChannel channel, U8 gain) {
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS(static_cast<U8>(channel)) | ADC_INPUTCTRL_MUXNEG_GND |
                         ADC_INPUTCTRL_GAIN(gain);
    AdcHal::waitForAdcSync();
}

void AdcHal::startConversion() {
    ADC->SWTRIG.bit.START = 1;
}

U32 AdcHal::readResult() {
    // Reading RESULT automatically clears INTFLAG.RESRDY per SAMD21 datasheet §33.6.5
    return ADC->RESULT.reg;
}

bool AdcHal::isResultReady() {
    return ADC->INTFLAG.bit.RESRDY;
}

bool AdcHal::isOverrun() {
    return ADC->INTFLAG.bit.OVERRUN;
}

void AdcHal::clearOverrun() {
    // Write-1-to-clear per SAMD21 datasheet §33.8.5
    ADC->INTFLAG.reg = ADC_INTFLAG_OVERRUN;
}

void AdcHal::enableInterrupt() {
    // Set to lowest priority (hardware-specific calculation)
    static constexpr U32 LOWEST_PRIORITY = (1U << __NVIC_PRIO_BITS) - 1;
    NVIC_SetPriority(ADC_IRQn, LOWEST_PRIORITY);
    NVIC_EnableIRQ(ADC_IRQn);
}

void AdcHal::enableAdcInterrupts() {
    ADC->INTENSET.reg = ADC_INTENSET_RESRDY | ADC_INTENSET_OVERRUN;
}

}  // namespace AdcHardware
}  // namespace Samd21

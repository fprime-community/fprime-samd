// ======================================================================
// \title  AdcDriver.cpp
// \author crsmith
// \brief  cpp file for AdcDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/AdcDriver.hpp"
#include "fprime-samd/Drv/Types/PinMux.hpp"
#include "sam.h"

namespace Samd21 {

// Singleton instance for ISR access
AdcDriver* AdcDriver::s_instance = nullptr;

// ----------------------------------------------------------------------
// Helper functions for synchronization waits
// ----------------------------------------------------------------------

static void waitForGclkSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForAdcSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && ADC->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AdcDriver::AdcDriver(const char* const compName)
    : AdcDriverComponentBase(compName),
      m_configured(false),
      m_gain(0),
      m_conversionPending(false),
      m_conversionComplete(false),
      m_overrunOccurred(false),
      m_lastResult(0),
      m_pendingPortNum(0) {
    // Initialize channel arrays
    for (FwIndexType i = 0; i < ADC_CHANNEL_COUNT; i++) {
        m_channels[i] = AdcChannel::AIN0;
        m_channelConfigured[i] = false;
    }
    // Note: s_instance is set in configure() to ensure only configured driver handles interrupts
}

AdcDriver::~AdcDriver() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

// ----------------------------------------------------------------------
// Configuration methods
// ----------------------------------------------------------------------
// Convenient conversion time calculator https://blog.thea.codes/getting-the-most-out-of-the-samd21-adc/
void AdcDriver::configure(VoltageReference ref, Resolution res, SampleCount samples, U8 samplingTime, Gain gain) {
    // Enable the APB clock for the ADC
    PM->APBCMASK.reg |= PM_APBCMASK_ADC;

    // Enable generic clock for ADC (use GCLK3 or GCLK0 depending on setup)
    // Using GCLK0 (48MHz) with DIV32 prescaler = 1.5MHz ADC clock
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_ADC;
    waitForGclkSync();

    // Load calibration data from NVM
    U32 bias = (*((U32*)ADC_FUSES_BIASCAL_ADDR) & ADC_FUSES_BIASCAL_Msk) >> ADC_FUSES_BIASCAL_Pos;
    U32 linearity = (*((U32*)ADC_FUSES_LINEARITY_0_ADDR) & ADC_FUSES_LINEARITY_0_Msk) >> ADC_FUSES_LINEARITY_0_Pos;
    linearity |= ((*((U32*)ADC_FUSES_LINEARITY_1_ADDR) & ADC_FUSES_LINEARITY_1_Msk) >> ADC_FUSES_LINEARITY_1_Pos) << 5;

    // Software reset to ensure clean state
    ADC->CTRLA.bit.SWRST = 1;
    waitForAdcSync();

    // Write calibration data
    ADC->CALIB.reg = ADC_CALIB_BIAS_CAL(bias) | ADC_CALIB_LINEARITY_CAL(linearity);

    // Configure voltage reference
    U8 refsel = 0;
    switch (ref) {
        case VoltageReference::INT1V:
            refsel = ADC_REFCTRL_REFSEL_INT1V_Val;
            break;
        case VoltageReference::INTVCC0:
            refsel = ADC_REFCTRL_REFSEL_INTVCC0_Val;
            break;
        case VoltageReference::INTVCC1:
            refsel = ADC_REFCTRL_REFSEL_INTVCC1_Val;
            break;
        case VoltageReference::VREFA:
            refsel = ADC_REFCTRL_REFSEL_AREFA_Val;
            break;
        case VoltageReference::VREFB:
            refsel = ADC_REFCTRL_REFSEL_AREFB_Val;
            break;
    }
    ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL(refsel) | ADC_REFCTRL_REFCOMP;
    waitForAdcSync();

    // Store gain for use in readAdc
    m_gain = static_cast<U8>(gain);

    // Configure resolution and prescaler
    U8 ressel = 0;
    switch (res) {
        case Resolution::RES_8BIT:
            ressel = ADC_CTRLB_RESSEL_8BIT_Val;
            break;
        case Resolution::RES_10BIT:
            ressel = ADC_CTRLB_RESSEL_10BIT_Val;
            break;
        case Resolution::RES_12BIT:
            ressel = ADC_CTRLB_RESSEL_12BIT_Val;
            break;
    }
    // Prescaler could be changed, DIV32 was selected because GCLK0 = 48MHz, with a prescaler of DIV32
    // the ADC clock runs at 1.5MHz which is close to, but less than the the max ADC clock freq of 2.1MHz
    ADC->CTRLB.reg = ADC_CTRLB_RESSEL(ressel) | ADC_CTRLB_PRESCALER_DIV32;
    waitForAdcSync();

    // Configure averaging (always write to ensure known state)
    U8 samplenum = static_cast<U8>(samples);
    // Calculate right shift for result adjustment (maintains resolution)
    // For SAMPLES_1 (no averaging), samplenum=0, adjres=0, disables averaging
    U8 adjres = samplenum;
    ADC->AVGCTRL.reg = ADC_AVGCTRL_SAMPLENUM(samplenum) | ADC_AVGCTRL_ADJRES(adjres);

    // Configure sampling time (SAMPLEN is 6-bit field, valid range 0-63)
    // These bits control the ADC sampling time in number of half CLK_ADC cycles, depending of the
    // prescaler value, thus controlling the ADC input impedance. Sampling time is set according to the
    // equation:
    // Sampling time = (SAMPLEN + 1) * (CLKadc / 2)
    FW_ASSERT(samplingTime <= 63, samplingTime);
    ADC->SAMPCTRL.reg = ADC_SAMPCTRL_SAMPLEN(samplingTime);

    // Register singleton for ISR access BEFORE enabling interrupts
    s_instance = this;

    // Enable ADC first (must be enabled before configuring interrupts)
    ADC->CTRLA.bit.ENABLE = 1;
    waitForAdcSync();

    // Per SAMD21 datasheet Section 33.6.2.1: "The first conversion after the reference
    // is changed must not be used." Perform a dummy conversion BEFORE enabling interrupts.
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS_SCALEDIOVCC | ADC_INPUTCTRL_MUXNEG_GND | ADC_INPUTCTRL_GAIN(m_gain);
    waitForAdcSync();
    ADC->SWTRIG.bit.START = 1;
    while (!ADC->INTFLAG.bit.RESRDY) {
    }
    (void)ADC->RESULT.reg;                  // Read and discard
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;  // Clear flag

    // Clear any pending interrupt flags AFTER dummy conversion
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY | ADC_INTFLAG_OVERRUN;

    // Now enable interrupts for result ready and overrun
    ADC->INTENSET.reg = ADC_INTENSET_RESRDY | ADC_INTENSET_OVERRUN;

    // Enable ADC interrupt in NVIC with lowest priority
    static constexpr U32 LOWEST_PRIORITY = (1U << __NVIC_PRIO_BITS) - 1;
    NVIC_SetPriority(ADC_IRQn, LOWEST_PRIORITY);
    NVIC_EnableIRQ(ADC_IRQn);

    m_configured = true;
}

void AdcDriver::configureChannel(FwIndexType portNum, AdcChannel channel) {
    FW_ASSERT(m_configured);
    FW_ASSERT(portNum < ADC_CHANNEL_COUNT);
    FW_ASSERT(!m_channelConfigured[portNum]);

    // Note: Pin muxing should be configured in topology BEFORE calling this for external channels (AIN0-AIN19)
    // Use Samd21::PinMux::configure(PINMUX_xxx) in instances.fpp
    // Internal channels (TEMP, BANDGAP, SCALEDIOVCC) don't require pin configuration

    // Enable internal resources if needed (saves power by only enabling when used)
    if (channel == AdcChannel::TEMP) {
        SYSCTRL->VREF.reg |= SYSCTRL_VREF_TSEN;  // Enable temperature sensor
    } else if (channel == AdcChannel::BANDGAP) {
        SYSCTRL->VREF.reg |= SYSCTRL_VREF_BGOUTEN;  // Enable bandgap output
    }

    // Store channel mapping
    m_channels[portNum] = channel;
    m_channelConfigured[portNum] = true;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Samd21::AdcStatus AdcDriver::readAdc_handler(FwIndexType portNum) {
    // Validate configuration
    if (!m_configured) {
        return Samd21::AdcStatus::ADC_NOT_CONFIGURED;
    }
    if (!m_channelConfigured[portNum]) {
        return Samd21::AdcStatus::ADC_INVALID_CHANNEL;
    }

    // Check if ADC is already busy
    if (m_conversionPending) {
        return Samd21::AdcStatus::ADC_BUSY;
    }

    // Select input channel with configured gain
    ADC->INPUTCTRL.reg = ADC_INPUTCTRL_MUXPOS(static_cast<U8>(m_channels[portNum])) | ADC_INPUTCTRL_MUXNEG_GND |
                         ADC_INPUTCTRL_GAIN(m_gain);
    waitForAdcSync();

    // Clear previous conversion state and mark as pending
    m_conversionComplete = false;
    m_overrunOccurred = false;
    m_conversionPending = true;
    m_pendingPortNum = portNum;

    // Start conversion (interrupt will fire when complete)
    ADC->SWTRIG.bit.START = 1;

    return Samd21::AdcStatus::ADC_OK;
}

void AdcDriver::schedIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    // Check if there's a pending conversion
    if (!m_conversionPending) {
        return;
    }

    // Check if conversion has completed
    if (m_conversionComplete) {
        // Deliver result to the requesting port via output port
        this->adcResult_out(m_pendingPortNum, m_lastResult);

        // Clear pending state - ADC is now available for new conversion
        m_conversionPending = false;
        m_conversionComplete = false;
        m_overrunOccurred = false;
    }
}

// ----------------------------------------------------------------------
// Interrupt handler
// ----------------------------------------------------------------------

void AdcDriver::handleInterrupt() {
    // Check for overrun error first
    if (ADC->INTFLAG.bit.OVERRUN) {
        // Clear overrun flag
        ADC->INTFLAG.reg = ADC_INTFLAG_OVERRUN;

        // Set error flag
        m_overrunOccurred = true;
    }

    // Check if RESRDY flag is set (this happens even if OVERRUN occurred)
    if (ADC->INTFLAG.bit.RESRDY) {
        // Read result (reading RESULT automatically clears RESRDY flag)
        m_lastResult = ADC->RESULT.reg;

        // Signal completion
        m_conversionComplete = true;
    }
}

}  // namespace Samd21

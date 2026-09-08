// ======================================================================
// \title  AdcDriverIsr.cpp
// \author crsmith
// \brief  ISR entry point for ADC driver
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/AdcDriver.hpp"
#include "sam.h"

// ----------------------------------------------------------------------
// ISR entry point (called by hardware)
// ----------------------------------------------------------------------

extern "C" void ADC_Handler(void) {
    // DON'T clear RESRDY here - reading RESULT clears it automatically
    // Only clear OVERRUN if it's set
    if (ADC->INTFLAG.bit.OVERRUN) {
        ADC->INTFLAG.reg = ADC_INTFLAG_OVERRUN;
    }

    if (Samd21::AdcDriver::s_instance != nullptr) {
        Samd21::AdcDriver::s_instance->handleInterrupt();
    }
}

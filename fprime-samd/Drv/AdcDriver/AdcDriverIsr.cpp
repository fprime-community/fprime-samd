// ======================================================================
// \title  AdcDriverIsr.cpp
// \author crsmith
// \brief  ISR entry point for ADC driver
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/AdcDriver.hpp"

// ----------------------------------------------------------------------
// ISR entry point (called by hardware)
// ----------------------------------------------------------------------

extern "C" void ADC_Handler(void) {
    // Forward to component's interrupt handler
    // All flag checking and clearing is done in handleInterrupt()
    if (Samd21::AdcDriver::s_instance != nullptr) {
        Samd21::AdcDriver::s_instance->handleInterrupt();
    }
}

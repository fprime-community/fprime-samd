#include "fprime-samd/Drv/DmaDriver/DmaDriver.hpp"

// ----------------------------------------------------------------------
// ISR entry point (called by hardware)
// ----------------------------------------------------------------------

extern "C" __attribute__((used)) void DMAC_Handler(void) {
    if (Samd21::DmaDriver::s_instance != nullptr) {
        Samd21::DmaDriver::s_instance->handleInterrupt();
    }
}

// ======================================================================
// \title  Idle.cpp
// \brief  SAMD21 implementation of Samd21::Idle
// ======================================================================

#include "fprime-samd/Drv/Types/Idle.hpp"
#include "fprime-samd/Mcu/SystemInit.h"
#include "sam.h"

namespace Samd21 {

void Idle::arm() {
    samd21_irq_taken = 0U;
}

void Idle::sleepUnlessInterrupted() {
    // Check and WFI are one unit: an interrupt arriving in between stays pending, ends
    // the WFI at once (wake-up does not require PRIMASK clear) and runs after enable.
    __disable_irq();
    if (samd21_irq_taken == 0U) {
        __DSB();
        __WFI();
    }
    __enable_irq();
}

}  // namespace Samd21

// ======================================================================
// \title  CriticalSectionStub.cpp
// \author tumbar
// \brief  Stub CriticalSection implementation for Linux/test builds
//
// This file is compiled for Linux/test builds so that components depending on
// fprime-samd_Drv_Types can link under native unit testing without the SAMD21
// CMSIS intrinsics (__disable_irq/__get_PRIMASK/etc.). For SAMD21 target
// builds, CriticalSection.cpp is used instead.
//
// Unit tests run single-threaded with no ISRs, so a critical section is a
// no-op: nesting is still reference-counted for parity with the real
// implementation, but no interrupts exist to disable.
// ======================================================================

#include "fprime-samd/Drv/Types/CriticalSection.hpp"
#include "Fw/FPrimeBasicTypes.hpp"

namespace Samd21 {

//! Depth of the currently-held critical section nesting
static U32 s_critical_section_counter = 0;

void CriticalSection::enter() {
    s_critical_section_counter++;
}

void CriticalSection::leave() {
    if (s_critical_section_counter > 0) {
        s_critical_section_counter--;
    }
}

CriticalSection::CriticalSection() {
    CriticalSection::enter();
}

CriticalSection::~CriticalSection() {
    CriticalSection::leave();
}

}  // namespace Samd21

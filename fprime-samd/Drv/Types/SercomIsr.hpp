// ======================================================================
// \title  SercomIsr.hpp
// \author tumbar
// \brief  hpp file for SERCOM ISR utilities
// ======================================================================

#ifndef Samd21_SercomIsr_HPP
#define Samd21_SercomIsr_HPP

#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"

namespace Samd21 {
class SercomIsr {
  public:
    //! Register a function handler for the a SERCOM interrupt
    //! Only one handler can be registered per SERCOM device.
    //! If a handler has already been registered, this function will assert.
    static void registerHandler(SercomKind sercom, void (*callback)(SercomKind, void*), void* data);
};
}  // namespace Samd21

#endif
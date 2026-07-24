// ======================================================================
// \title  Sercom.hpp
// \author tumbar
// \brief  hpp file for SERCOM utilities
// ======================================================================

#ifndef Samd21_SercomIsr_HPP
#define Samd21_SercomIsr_HPP

#include "fprime-samd/Drv/Types/TriggerSourceEnumAc.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"

#ifdef __SAMD21__
#include "samd.h"
#endif

namespace Samd21 {
class SercomUtil {
  public:
    //! Get the RX DMA trigger source for a given SERCOM device
    static Dma::TriggerSource rxDmaTrigger(SercomKind sercom);

    //! Get the TX DMA trigger source for a given SERCOM device
    static Dma::TriggerSource txDmaTrigger(SercomKind sercom);

    // Hardware only declarations
#ifdef __SAMD21__
    //! Register a function handler for the a SERCOM interrupt
    //! Only one handler can be registered per SERCOM device.
    //! If a handler has already been registered, this function will assert.
    static void registerIsrHandler(SercomKind sercom, void (*callback)(SercomKind, void*), void* data);

    //! Get the pointer to the base register of this SERCOM device
    static ::Sercom* getHardware(SercomKind sercom);
#endif
};

}  // namespace Samd21

#endif
// ======================================================================
// \title  PinMux.hpp
// \author tumbar
// \brief  hpp file for PinMux utilities
// ======================================================================

#ifndef Samd21_PinMux_HPP
#define Samd21_PinMux_HPP

#if defined(__SAMD21E15A__) || defined(__ATSAMD21E15A__)
#include "samd21/include/pio/samd21e15a.h"
#elif defined(__SAMD21E16A__) || defined(__ATSAMD21E16A__)
#include "samd21/include/pio/samd21e16a.h"
#elif defined(__SAMD21E17A__) || defined(__ATSAMD21E17A__)
#include "samd21/include/pio/samd21e17a.h"
#elif defined(__SAMD21E18A__) || defined(__ATSAMD21E18A__)
#include "samd21/include/pio/samd21e18a.h"
#elif defined(__SAMD21G15A__) || defined(__ATSAMD21G15A__)
#include "samd21/include/pio/samd21g15a.h"
#elif defined(__SAMD21G16A__) || defined(__ATSAMD21G16A__)
#include "samd21/include/pio/samd21g16a.h"
#elif defined(__SAMD21G17A__) || defined(__ATSAMD21G17A__)
#include "samd21/include/pio/samd21g17a.h"
#elif defined(__SAMD21G17AU__) || defined(__ATSAMD21G17AU__)
#include "samd21/include/pio/samd21g17au.h"
#elif defined(__SAMD21G18A__) || defined(__ATSAMD21G18A__)
#include "samd21/include/pio/samd21g18a.h"
#elif defined(__SAMD21G18AU__) || defined(__ATSAMD21G18AU__)
#include "samd21/include/pio/samd21g18au.h"
#elif defined(__SAMD21J15A__) || defined(__ATSAMD21J15A__)
#include "samd21/include/pio/samd21j15a.h"
#elif defined(__SAMD21J16A__) || defined(__ATSAMD21J16A__)
#include "samd21/include/pio/samd21j16a.h"
#elif defined(__SAMD21J16AC__) || defined(__ATSAMD21J16AC__)
#include "samd21/include/pio/samd21j16ac.h"
#elif defined(__SAMD21J17A__) || defined(__ATSAMD21J17A__)
#include "samd21/include/pio/samd21j17a.h"
#elif defined(__SAMD21J17AC__) || defined(__ATSAMD21J17AC__)
#include "samd21/include/pio/samd21j17ac.h"
#elif defined(__SAMD21J18A__) || defined(__ATSAMD21J18A__)
#include "samd21/include/pio/samd21j18a.h"
#elif defined(__SAMD21J18AC__) || defined(__ATSAMD21J18AC__)
#include "samd21/include/pio/samd21j18ac.h"
#else
#error Library does not support the specified device.
#endif

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
class PinMux {
  public:
    /// Configure a pin peripheral function using one of the `PINMUX_*` definitions in the ATMEL CMSIS macros in PIO
    static void configure(U32 pin_mux_cfg);
};
}  // namespace Samd21

#endif

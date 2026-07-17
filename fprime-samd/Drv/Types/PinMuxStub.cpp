// ======================================================================
// \title  PinMuxStub.cpp
// \author tumbar
// \brief  Stub PinMux implementation for Linux/test builds
//
// This file is compiled for Linux/test builds so that components depending on
// fprime-samd_Drv_Types can link under native unit testing without pulling in
// the SAMD21 CMSIS device headers. For SAMD21 target builds, PinMux.cpp is
// used instead.
// ======================================================================

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
class PinMux {
  public:
    static void configure(U32 pin_mux_cfg);
};

void PinMux::configure(U32 pin_mux_cfg) {
    // Stub: no-op for tests (no hardware PORT registers on the host)
    (void)pin_mux_cfg;
}
}  // namespace Samd21

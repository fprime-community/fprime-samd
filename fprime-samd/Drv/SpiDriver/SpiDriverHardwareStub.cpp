// ======================================================================
// \title  SpiDriverHardwareStub.cpp
// \brief  Stub hardware implementation for the SPI peripheral (Linux/test builds)
//
// Compiled for native/test builds so SpiDriver can be unit tested. SAMD21
// target builds use SpiDriverHardware.cpp instead. The stub records the
// arguments of each hardware call; the DMA traffic itself is visible to the
// tests through the component's ports.
// ======================================================================
#include "fprime-samd/Drv/SpiDriver/SpiDriverHardware.hpp"

namespace Samd21 {
namespace SpiHardware {

static StubState s_state = {};

StubState& getStubState() {
    return s_state;
}

void resetStubState() {
    // Value-initialize rather than memset: SercomKind is an autocoded class.
    s_state = StubState{};
}

void SpiHal::configure(SercomKind sercom,
                       U32 baud_rate_khz,
                       SpiDriver::DataOrder data_order,
                       SpiDriver::ClockPolarity clock_polarity,
                       SpiDriver::ClockPhase clock_phase,
                       SpiDriver::DataInPinout data_in_pinout,
                       SpiDriver::DataOutPinout data_out_pinout,
                       SpiDriver::RunInStandby run_in_standby,
                       SpiDriver::HardwareChipSelect hardware_chipselect) {
    s_state.configured = true;
    s_state.configure_count++;
    s_state.sercom = sercom;
    s_state.baud_rate_khz = baud_rate_khz;
    s_state.data_order = data_order;
    s_state.clock_polarity = clock_polarity;
    s_state.clock_phase = clock_phase;
    s_state.data_in_pinout = data_in_pinout;
    s_state.data_out_pinout = data_out_pinout;
    s_state.run_in_standby = run_in_standby;
    s_state.hardware_chipselect = hardware_chipselect;
}

U32 SpiHal::getDataRegisterAddress(SercomKind sercom) {
    (void)sercom;
    return s_state.data_register_address;
}

}  // namespace SpiHardware
}  // namespace Samd21

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

void fireIsr() {
    if (s_state.isr_callback != nullptr) {
        s_state.isr_callback(s_state.isr_data, s_state.isr_sercom);
    }
}

void SpiHal::registerIsr(SercomKind sercom,
                         void (*callback)(Fw::PassiveComponentBase*, SercomKind),
                         Fw::PassiveComponentBase* data) {
    s_state.isr_callback = callback;
    s_state.isr_data = data;
    s_state.isr_sercom = sercom;
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
    s_state.enabled = true;
}

void SpiHal::setBaudRate(SercomKind sercom, U32 baud_rate_khz) {
    // Mirror the real HAL: BAUD is enable-protected, so the SERCOM is disabled around the
    // write and must come back enabled or every later transfer is lost.
    s_state.enabled = false;
    s_state.baud_rate_khz = baud_rate_khz;
    s_state.set_baud_count++;
    s_state.enabled = true;
    s_state.enable_count++;
}

U32 SpiHal::getDataRegisterAddress(SercomKind sercom) {
    (void)sercom;
    // Called when a transfer is started: a disabled SERCOM would never clock the DMA
    if (!s_state.enabled) {
        s_state.data_register_reads_while_disabled++;
    }
    return s_state.data_register_address;
}

SpiErrorStatus SpiHal::readErrorStatus(SercomKind sercom) {
    (void)sercom;
    return s_state.error_status;
}

void SpiHal::acknowledgeError(SercomKind sercom) {
    (void)sercom;
    s_state.acknowledge_error_count++;
    // Model the write-1-clear: nothing is pending after the acknowledge
    s_state.error_status = SpiErrorStatus{};
}

}  // namespace SpiHardware
}  // namespace Samd21

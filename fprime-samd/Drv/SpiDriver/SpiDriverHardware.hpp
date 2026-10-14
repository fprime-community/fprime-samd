// ======================================================================
// \title  SpiDriverHardware.hpp
// \brief  Hardware abstraction layer for the SERCOM SPI host peripheral
//
// All raw SERCOM SPI register access (clock gating, GCLK routing, CTRLA/CTRLB,
// BAUD, enable) lives behind this interface so the SpiDriver component logic
// (busy state, chip select, DMA sequencing, replies) can be exercised natively
// against a stub. Mirrors the I2c/Usart driver HAL split.
// ======================================================================
#ifndef Samd21_SpiDriverHardware_HPP
#define Samd21_SpiDriverHardware_HPP

#include "fprime-samd/Drv/SpiDriver/SpiDriver.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"

namespace Samd21 {
namespace SpiHardware {

//! Hardware abstraction layer for the SERCOM SPI host peripheral.
struct SpiHal {
    //! Configure and enable the SERCOM peripheral for SPI host operation.
    //!
    //! Performs the §27.6.2.1 initialization sequence (clock gating, GCLK
    //! routing, software reset, CTRLA/CTRLB, BAUD, enable). All sync waits are
    //! bounded. Asserts on an unknown SERCOM or an unachievable baud rate.
    static void configure(SercomKind sercom,
                          U32 baud_rate_khz,
                          SpiDriver::DataOrder data_order,
                          SpiDriver::ClockPolarity clock_polarity,
                          SpiDriver::ClockPhase clock_phase,
                          SpiDriver::DataInPinout data_in_pinout,
                          SpiDriver::DataOutPinout data_out_pinout,
                          SpiDriver::RunInStandby run_in_standby,
                          SpiDriver::HardwareChipSelect hardware_chipselect);

    //! Address of the SERCOM SPI DATA register (DMA source for MISO, destination for MOSI).
    static U32 getDataRegisterAddress(SercomKind sercom);
};

//! Test-only hooks into the stub hardware implementation.
//! Compiled only for native/test builds (not the SAMD21 target); lets unit
//! tests observe HAL arguments and choose the DATA register address.
#ifndef __SAMD21__
struct StubState {
    // configure() capture
    bool configured;
    U32 configure_count;
    SercomKind sercom;
    U32 baud_rate_khz;
    SpiDriver::DataOrder data_order;
    SpiDriver::ClockPolarity clock_polarity;
    SpiDriver::ClockPhase clock_phase;
    SpiDriver::DataInPinout data_in_pinout;
    SpiDriver::DataOutPinout data_out_pinout;
    SpiDriver::RunInStandby run_in_standby;
    SpiDriver::HardwareChipSelect hardware_chipselect;

    // getDataRegisterAddress() control
    U32 data_register_address;
};

//! Get the mutable stub state (shared across all HAL calls)
StubState& getStubState();

//! Reset the stub state for a clean test run
void resetStubState();
#endif

}  // namespace SpiHardware
}  // namespace Samd21

#endif

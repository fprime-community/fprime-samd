// ======================================================================
// \title  I2cDriverHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for I2C driver peripheral operations
// ======================================================================

#ifndef Samd21_I2cDriverHardware_HPP
#define Samd21_I2cDriverHardware_HPP

#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"

namespace Samd21 {
namespace I2cHardware {

struct I2cHal {
    //! Configure and enable the SERCOM I2C peripheral in Host (master) mode
    //!
    //! Performs the full §29.6.2.1 initialization sequence (clock gating, GCLK
    //! routing, CTRLA/CTRLB, baud rate) and returns once the peripheral is
    //! enabled and the bus state has been forced IDLE. All hardware
    //! synchronization waits are bounded to ~1s.
    static void configure(SercomKind sercom,
                          I2cDriver::SclLowTimeout scl_low_timeout,
                          I2cDriver::InactiveTimeout inactive_timeout,
                          I2cDriver::ClockStretchMode clock_stretch_mode,
                          I2cDriver::Frequency frequency,
                          I2cDriver::ClientSclLowTimeout client_scl_low_timeout,
                          I2cDriver::HostSclLowTimeout host_scl_low_timeout,
                          I2cDriver::SdaHold sda_hold,
                          I2cDriver::PinUsage pin_usage,
                          I2cDriver::RunInStandby run_in_standby);
};
}  // namespace I2cHardware
}  // namespace Samd21

#endif

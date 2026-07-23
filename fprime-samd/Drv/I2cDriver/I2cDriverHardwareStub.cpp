// ======================================================================
// \title  I2cDriverHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for I2C peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, I2cDriverHardware.cpp is used instead.
//
// The stub records the arguments of the last hardware call so unit tests can
// verify what the driver asked the peripheral to do without any real MCU.
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriverHardware.hpp"

namespace Samd21 {
namespace I2cHardware {

void I2cHal::configure(SercomKind sercom,
                       I2cDriver::SclLowTimeout scl_low_timeout,
                       I2cDriver::InactiveTimeout inactive_timeout,
                       I2cDriver::ClockStretchMode clock_stretch_mode,
                       I2cDriver::Frequency frequency,
                       I2cDriver::ClientSclLowTimeout client_scl_low_timeout,
                       I2cDriver::HostSclLowTimeout host_scl_low_timeout,
                       I2cDriver::SdaHold sda_hold,
                       I2cDriver::PinUsage pin_usage,
                       I2cDriver::RunInStandby run_in_standby) {}

}  // namespace I2cHardware
}  // namespace Samd21

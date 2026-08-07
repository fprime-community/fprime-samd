/*
 * I2cDriverConfig.hpp:
 *
 * Configuration settings for the SAMD21 I2C driver component.
 */

#ifndef SAMD21_I2C_DRIVER_CFG_HPP_
#define SAMD21_I2C_DRIVER_CFG_HPP_

namespace Samd21 {
enum I2cDriverConfig {
    //! Enable the diagnostic telemetry channels (bus state, clock hold, RX-NACK,
    //! device-on-bus). When 0, only the always-on bus-error counter is emitted;
    //! the periodic bus-status read is skipped entirely. Set to 0 to save the
    //! per-tick hardware read and downlink bandwidth once a bus is brought up.
    I2C_ENABLE_DEBUG_TELEMETRY = 1,
};
}  // namespace Samd21

#endif

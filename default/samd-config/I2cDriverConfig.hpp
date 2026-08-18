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

    //! Watchdog threshold, in reportTelemetryIn ticks, before a transaction that
    //! never completed is declared stuck and force-recovered. A healthy
    //! transaction finishes in milliseconds, far inside one tick; a transaction
    //! still in flight across this many consecutive ticks has lost its completion
    //! interrupt (see recoverFromStall). Must be >= 1. At the FFB_Tester 1 Hz tick
    //! this is a ~3 s grace period.
    I2C_STALL_RECOVERY_TICKS = 3,
};
}  // namespace Samd21

#endif

/*
 * SpiDriverConfig.hpp:
 *
 * Configuration settings for the SAMD21 SPI driver component.
 */
#ifndef SAMD21_SPI_DRIVER_CFG_HPP_
#define SAMD21_SPI_DRIVER_CFG_HPP_

namespace Samd21 {
enum SpiDriverConfig {
    //! Non-zero: activeIn returns true while a transfer is in flight at the moment it returns, so the
    //! Svc.PassiveCycler re-polls (and the deployment does not sleep) until the outcome is latched and
    //! delivered. Zero: activeIn returns true only when a reply record was latched during the call; a
    //! completion that lands after the pass waits for the next interrupt (e.g. the RTC tick).
    SPI_REPOLL_DURING_OUTSTANDING_TRANSACTION = 1,
};
}  // namespace Samd21

#endif  // SAMD21_SPI_DRIVER_CFG_HPP_

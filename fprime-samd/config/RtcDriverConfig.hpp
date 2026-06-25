/*
 * RtcDriverConfig.hpp:
 *
 * Configuration settings for the SAMD21 realtime clock driver component.
 */

#ifndef SAMD21_RTC_DRIVER_CFG_HPP_
#define SAMD21_RTC_DRIVER_CFG_HPP_

namespace Samd21 {
enum RtcDriverConfig {
    //! Frequency of the external clock in Hz
    EXTERNAL_CLOCK_FREQ_HZ = 32768,
};
}  // namespace Samd21

#endif

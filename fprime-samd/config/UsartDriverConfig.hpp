/*
 * UsartDriverConfig.hpp:
 *
 * Configuration settings for the SAMD21 UART driver component.
 */

#ifndef SAMD21_USART_DRIVER_CFG_HPP_
#define SAMD21_USART_DRIVER_CFG_HPP_

namespace Samd21 {
enum UsartDriverConfig {
    //!< Length of the TX job queue
    USART_TX_BUFFER_N = 2,
};
}  // namespace Samd21

#endif

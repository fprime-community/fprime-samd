/*
 * UsartDriverConfig.hpp:
 *
 * Configuration settings for the SAMD21 UART driver component.
 */

#ifndef SAMD21_USART_DRIVER_CFG_HPP_
#define SAMD21_USART_DRIVER_CFG_HPP_

namespace Samd21 {
enum UsartDriverConfig {
    //! Length of the TX job queue
    USART_TX_BUFFER_N = 2,

    //! Length of each Rx buffer
    //! Each USART driver will allocate two Rx buffers
    USART_RX_BUFFER_SIZE = 256,

    //! Length of the USART event queue
    USART_QUEUE_DEPTH = 4,
};
}  // namespace Samd21

#endif

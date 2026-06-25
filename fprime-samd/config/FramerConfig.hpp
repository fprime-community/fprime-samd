/*
 * FramerConfig.hpp:
 *
 * Configuration settings for the SAMD21 Framer component.
 */

#ifndef SAMD21_FRAMER_CFG_HPP_
#define SAMD21_FRAMER_CFG_HPP_

namespace Samd21 {
enum FramerConfig {
    //! Maximum size for a single transmit buffer (bytes)
    //! Buffer contains: frame header + multiple accumulated ComBuffers + frame trailer
    //! Larger buffers allow more packet accumulation before flush
    FRAMER_TX_BUFFER_SIZE = 256,
};
}  // namespace Samd21

#endif

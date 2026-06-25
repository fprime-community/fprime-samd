/*
 * UsartDriverConfig.hpp:
 *
 * Configuration settings for the SAMD21 UART driver component.
 */

#ifndef SAMD21_DMA_DRIVER_CFG_HPP_
#define SAMD21_DMA_DRIVER_CFG_HPP_

namespace Samd21 {
enum DmaDriverConfig {
    //! Number of DMA descriptors allocated to the driver.
    //! These descriptors are shared across all channels.
    //! They mark the number of
    DMA_DESCRIPTOR_N = 16,
};

// We are limited in the DmaDriver by how many descriptors can be used
// since free/used descriptors are tracked in a U32 bitfield
static_assert(DmaDriverConfig::DMA_DESCRIPTOR_N <= 32, "Too many DMAC descriptors");

}  // namespace Samd21

#endif

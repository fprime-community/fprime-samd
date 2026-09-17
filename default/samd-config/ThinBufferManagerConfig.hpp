/*
 * ThinBufferManagerConfig.hpp:
 *
 * Configuration settings for the SAMD21 ThinBufferManager component.
 */

#ifndef SAMD21_THINBUFFERMANAGER_CFG_HPP_
#define SAMD21_THINBUFFERMANAGER_CFG_HPP_

namespace Samd21 {
enum ThinBufferManagerConfig {
    //! Bound on the number of bins a ThinBufferManager instance's BufferBins table can hold
    THINBUFFERMGR_MAX_NUM_BINS = 3,
};
}  // namespace Samd21

#endif

/*
 * StaticPrmDbConfig.hpp:
 *
 * Configuration settings for the SAMD21 static parameter database component.
 */
#ifndef SAMD21_STATIC_PRM_DB_CFG_HPP_
#define SAMD21_STATIC_PRM_DB_CFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace StaticPrmDbConfig {

//! Compile-time check of the database image against the deployment's storage. The
//! autocoded database static_asserts this with the image size (StaticPrmDb::getDatabaseBuffer),
//! so a layout that cannot hold the image (e.g. overlapping banks) fails the build. Override this
//! header to check the deployment's storage layout; the default accepts any size.
constexpr bool storageFits(FwSizeType imageSize) {
    return (static_cast<void>(imageSize), true);
}

}  // namespace StaticPrmDbConfig
}  // namespace Samd21

#endif  // SAMD21_STATIC_PRM_DB_CFG_HPP_

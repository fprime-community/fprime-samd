/*
 * StaticPrmDbConfig.hpp:
 *
 * Configuration settings for the SAMD21 static parameter database component. The record value
 * size (RECORD_VALUE_SIZE) is an FPP constant in Samd21StaticPrmDbConfig.fpp, since the
 * autocoder needs it too; the record size and every size below derive from it.
 *
 * PrmStorage checks the store (STORE_RECORDS records) against its bank layout at build time.
 */
#ifndef SAMD21_STATIC_PRM_DB_CFG_HPP_
#define SAMD21_STATIC_PRM_DB_CFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace StaticPrmDbConfig {

//! Records moved per save/load transfer. Sizes the scratch buffer (SCRATCH_RECORDS * record size).
constexpr FwSizeType SCRATCH_RECORDS = 5;

//! Record slots in the persistent store. The store size is fixed so it does not change with the
//! dictionary: a store saved by an older dictionary is read in full. Must hold every record of
//! the deployment (checked by the autocoded database) and fit the storage (checked by PrmStorage).
constexpr FwSizeType STORE_RECORDS = 64;

}  // namespace StaticPrmDbConfig
}  // namespace Samd21

#endif  // SAMD21_STATIC_PRM_DB_CFG_HPP_

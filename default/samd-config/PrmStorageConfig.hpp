// ======================================================================
// \title  PrmStorageConfig.hpp
// \brief  F-RAM bank layout and retry limit of Samd21::PrmStorage
// ======================================================================
#ifndef SAMD21_PRM_STORAGE_CFG_HPP_
#define SAMD21_PRM_STORAGE_CFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace PrmStorageConfig {

//! F-RAM offset of bank 0. Each bank holds one parameter store
//! (StaticPrmDbConfig::STORE_RECORDS records), and the banks are packed back to
//! back from here. PrmStorage checks at build time that they fit the F-RAM
//! (PrmStorageLayout.hpp).
static constexpr U32 BASE_OFFSET = 0x0000;

//! Redundant copies of the parameter store (at least 1). Bank 0 is selected at boot.
static constexpr U8 NUM_BANKS = 2;

//! schedIn ticks an F-RAM request rejected BUSY is retried before the transfer is failed
static constexpr U32 BUSY_RETRY_LIMIT = 8;

}  // namespace PrmStorageConfig
}  // namespace Samd21

#endif  // SAMD21_PRM_STORAGE_CFG_HPP_

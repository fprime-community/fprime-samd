// ======================================================================
// \title  PrmStorageConfig.hpp
// \brief  F-RAM bank layout and retry limit of Samd21::PrmStorage
// ======================================================================
#ifndef SAMD21_PRM_STORAGE_CFG_HPP_
#define SAMD21_PRM_STORAGE_CFG_HPP_

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace PrmStorageConfig {

//! F-RAM offset of the PRIME bank. Each bank holds one parameter store
//! (StaticPrmDbConfig::STORE_RECORDS records); PrmStorage checks at build time
//! that the banks hold it without overlapping (PrmStorageLayout.hpp).
static constexpr U32 PRIME_OFFSET = 0x0000;

//! F-RAM offset of the BACKUP bank
static constexpr U32 BACKUP_OFFSET = 0x1000;

//! schedIn ticks an F-RAM request rejected BUSY is retried before the transfer is failed
static constexpr U32 BUSY_RETRY_LIMIT = 8;

}  // namespace PrmStorageConfig
}  // namespace Samd21

#endif  // SAMD21_PRM_STORAGE_CFG_HPP_

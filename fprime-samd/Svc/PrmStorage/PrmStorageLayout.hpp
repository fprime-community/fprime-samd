// ======================================================================
// \title  PrmStorageLayout.hpp
// \author tumbar
// \brief  Compile-time check of the PrmStorage bank layout
// ======================================================================
#ifndef Samd21_PrmStorageLayout_HPP
#define Samd21_PrmStorageLayout_HPP

#include "FramConfig/FramConfig.hpp"
#include "samd-config/PrmStorageConfig.hpp"

namespace Samd21 {
namespace PrmStorageLayout {

//! True when a bank at offset holds storeSize bytes inside the F-RAM
constexpr bool bankFits(U32 offset, FwSizeType storeSize) {
    return (storeSize <= static_cast<FwSizeType>(Fram::FramConfig::Size)) &&
           (static_cast<FwSizeType>(offset) <= static_cast<FwSizeType>(Fram::FramConfig::Size) - storeSize);
}

//! True when both banks hold a store of storeSize bytes inside the F-RAM and
//! do not overlap. PrmStorage static_asserts this with the StaticPrmDbConfig store size.
constexpr bool banksHold(FwSizeType storeSize) {
    return bankFits(PrmStorageConfig::PRIME_OFFSET, storeSize) &&
           bankFits(PrmStorageConfig::BACKUP_OFFSET, storeSize) &&
           ((static_cast<FwSizeType>(PrmStorageConfig::PRIME_OFFSET) + storeSize <=
             static_cast<FwSizeType>(PrmStorageConfig::BACKUP_OFFSET)) ||
            (static_cast<FwSizeType>(PrmStorageConfig::BACKUP_OFFSET) + storeSize <=
             static_cast<FwSizeType>(PrmStorageConfig::PRIME_OFFSET)));
}

}  // namespace PrmStorageLayout
}  // namespace Samd21

#endif  // Samd21_PrmStorageLayout_HPP

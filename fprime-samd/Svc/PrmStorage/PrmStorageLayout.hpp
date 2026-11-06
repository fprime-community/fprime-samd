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

//! True when a bank at offset holds imageSize bytes inside the F-RAM
constexpr bool bankFits(U32 offset, FwSizeType imageSize) {
    return (imageSize <= static_cast<FwSizeType>(Fram::FramConfig::Size)) &&
           (static_cast<FwSizeType>(offset) <= static_cast<FwSizeType>(Fram::FramConfig::Size) - imageSize);
}

//! True when both banks hold an image of imageSize bytes inside the F-RAM and
//! do not overlap. The image size is only known to the autocoded parameter
//! database, which static_asserts this (see the StaticPrmDbConfig.hpp override).
constexpr bool banksHold(FwSizeType imageSize) {
    return bankFits(PrmStorageConfig::PRIME_OFFSET, imageSize) &&
           bankFits(PrmStorageConfig::BACKUP_OFFSET, imageSize) &&
           ((static_cast<FwSizeType>(PrmStorageConfig::PRIME_OFFSET) + imageSize <=
             static_cast<FwSizeType>(PrmStorageConfig::BACKUP_OFFSET)) ||
            (static_cast<FwSizeType>(PrmStorageConfig::BACKUP_OFFSET) + imageSize <=
             static_cast<FwSizeType>(PrmStorageConfig::PRIME_OFFSET)));
}

}  // namespace PrmStorageLayout
}  // namespace Samd21

#endif  // Samd21_PrmStorageLayout_HPP

// ======================================================================
// \title  PrmStorageLayout.hpp
// \author tumbar
// \brief  F-RAM bank layout of PrmStorage
// ======================================================================
#ifndef Samd21_PrmStorageLayout_HPP
#define Samd21_PrmStorageLayout_HPP

#include "FramConfig/FramConfig.hpp"
#include "fprime-samd/Svc/Types/PrmRecord.hpp"
#include "samd-config/PrmStorageConfig.hpp"
#include "samd-config/StaticPrmDbConfig.hpp"

namespace Samd21 {
namespace PrmStorageLayout {

//! Bytes of one bank: one parameter store of StaticPrmDbConfig::STORE_RECORDS records
constexpr FwSizeType BANK_SIZE = StaticPrmDbConfig::STORE_RECORDS * PrmRecord::SIZE;

//! F-RAM offset of a bank. The banks are packed back to back from PrmStorageConfig::BASE_OFFSET.
constexpr U32 bankOffset(U8 bank) {
    return static_cast<U32>(static_cast<FwSizeType>(PrmStorageConfig::BASE_OFFSET) +
                            (static_cast<FwSizeType>(bank) * BANK_SIZE));
}

//! True when there is at least one bank and all PrmStorageConfig::NUM_BANKS banks lie inside
//! the F-RAM. PrmStorage static_asserts this.
constexpr bool banksFit() {
    return (PrmStorageConfig::NUM_BANKS > 0) &&
           (static_cast<FwSizeType>(PrmStorageConfig::BASE_OFFSET) <=
            static_cast<FwSizeType>(Fram::FramConfig::Size)) &&
           (static_cast<FwSizeType>(PrmStorageConfig::NUM_BANKS) * BANK_SIZE <=
            static_cast<FwSizeType>(Fram::FramConfig::Size) - static_cast<FwSizeType>(PrmStorageConfig::BASE_OFFSET));
}

}  // namespace PrmStorageLayout
}  // namespace Samd21

#endif  // Samd21_PrmStorageLayout_HPP

// ======================================================================
// \title  PrmRecord.hpp
// \author tumbar
// \brief  CRC-guarded parameter record used by the static parameter database
// ======================================================================

#ifndef Samd21_PrmRecord_HPP
#define Samd21_PrmRecord_HPP

#include <cstddef>

#include "Fw/FPrimeBasicTypes.hpp"

namespace Samd21 {

//! MSB-first CRC-8 polynomial used for per-parameter checks (CRC-8/AUTOSAR)
constexpr U8 CRC8_POLY = 0x2F;

//! Shift a single byte through the MSB-first CRC-8 polynomial (bitwise, for compile-time use)
constexpr U8 crc8Byte(U8 crc, U8 byte) {
    crc = static_cast<U8>(crc ^ byte);
    for (FwIndexType b = 0; b < 8; b++) {
        crc = (crc & 0x80) ? static_cast<U8>((crc << 1) ^ CRC8_POLY) : static_cast<U8>(crc << 1);
    }
    return crc;
}

//! CRC-8 over a buffer starting from a seed (table driven, for runtime use)
U8 crc8(U8 seed, const U8* data, FwSizeType len);

//! A single parameter value with a CRC-8 guarding its contents
//!
//! The CRC is seeded with the parameter ID so records can't be swapped undetected.
template <size_t TY_SIZE, FwPrmIdType PRM_ID>
struct PrmRecord {
    U8 crc = 0;
    U8 data[TY_SIZE] = {};

    //! CRC seed derived from the parameter ID
    static constexpr U8 crcSeed() {
        U8 currentCrc = 0xFF;
        for (FwIndexType i = sizeof(FwPrmIdType) - 1; i >= 0; i--) {
            currentCrc = crc8Byte(currentCrc, static_cast<U8>(PRM_ID >> (8 * i)));
        }
        return currentCrc;
    }

    //! CRC over the record's current data
    U8 computeCrc() const {
        constexpr U8 seed = crcSeed();
        return crc8(seed, data, TY_SIZE);
    }
};

}  // namespace Samd21

#endif  // Samd21_PrmRecord_HPP

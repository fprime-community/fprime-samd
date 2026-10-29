// ======================================================================
// \title  PrmRecord.cpp
// \author tumbar
// \brief  CRC-8 implementation for the static parameter database records
// ======================================================================

#include "fprime-samd/Svc/Types/PrmRecord.hpp"

namespace Samd21 {

//! Shift a nibble (placed in the high bits) through 4 rounds of the MSB-first CRC-8 polynomial
static constexpr U8 crc8Nibble(U8 n) {
    U8 crc = static_cast<U8>(n << 4);
    for (U8 b = 0; b < 4; b++) {
        crc = (crc & 0x80) ? static_cast<U8>((crc << 1) ^ CRC8_POLY) : static_cast<U8>(crc << 1);
    }
    return crc;
}

// Nibble-table CRC-8: 16-byte table, ~2 lookups per byte on M0+
static constexpr U8 CRC8_NIBBLE[16] = {
    crc8Nibble(0x0), crc8Nibble(0x1), crc8Nibble(0x2), crc8Nibble(0x3), crc8Nibble(0x4), crc8Nibble(0x5),
    crc8Nibble(0x6), crc8Nibble(0x7), crc8Nibble(0x8), crc8Nibble(0x9), crc8Nibble(0xA), crc8Nibble(0xB),
    crc8Nibble(0xC), crc8Nibble(0xD), crc8Nibble(0xE), crc8Nibble(0xF),
};

U8 crc8(U8 seed, const U8* data, FwSizeType len) {
    U8 crc = seed ^ 0xFF;
    for (FwSizeType i = 0; i < len; i++) {
        crc ^= data[i];
        crc = static_cast<U8>((crc << 4) ^ CRC8_NIBBLE[crc >> 4]);
        crc = static_cast<U8>((crc << 4) ^ CRC8_NIBBLE[crc >> 4]);
    }
    return crc;
}

}  // namespace Samd21

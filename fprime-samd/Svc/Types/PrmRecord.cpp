// ======================================================================
// \title  PrmRecord.cpp
// \author tumbar
// \brief  CRC-16 and persistent record packing for the static parameter database
// ======================================================================

#include "fprime-samd/Svc/Types/PrmRecord.hpp"
#include "Fw/Types/Assert.hpp"

#include <cstring>

namespace Samd21 {

//! Shift a nibble (placed in the high bits) through 4 rounds of the CRC-16 polynomial
static constexpr U16 crc16Nibble(U8 n) {
    U16 crc = static_cast<U16>(static_cast<U16>(n) << 12);
    for (U8 b = 0; b < 4; b++) {
        crc = (crc & 0x8000) ? static_cast<U16>((crc << 1) ^ CRC16_POLY) : static_cast<U16>(crc << 1);
    }
    return crc;
}

// Nibble-table CRC-16: 32-byte table, ~2 lookups per byte on M0+
static constexpr U16 CRC16_NIBBLE[16] = {
    crc16Nibble(0x0), crc16Nibble(0x1), crc16Nibble(0x2), crc16Nibble(0x3), crc16Nibble(0x4), crc16Nibble(0x5),
    crc16Nibble(0x6), crc16Nibble(0x7), crc16Nibble(0x8), crc16Nibble(0x9), crc16Nibble(0xA), crc16Nibble(0xB),
    crc16Nibble(0xC), crc16Nibble(0xD), crc16Nibble(0xE), crc16Nibble(0xF),
};

U16 crc16(U16 crc, const U8* data, FwSizeType len) {
    for (FwSizeType i = 0; i < len; i++) {
        crc = static_cast<U16>(crc ^ (static_cast<U16>(data[i]) << 8));
        crc = static_cast<U16>((crc << 4) ^ CRC16_NIBBLE[crc >> 12]);
        crc = static_cast<U16>((crc << 4) ^ CRC16_NIBBLE[crc >> 12]);
    }
    return crc;
}

namespace PrmRecord {

U16 computeCrc(const U8* record) {
    // The ID is covered so a flipped ID cannot move a value onto another parameter
    U16 crc = crc16(CRC16_INIT, &record[ID_OFFSET], ID_SIZE);
    return crc16(crc, &record[VALUE_OFFSET], VALUE_SIZE);
}

U16 getId(const U8* record) {
    return static_cast<U16>((static_cast<U16>(record[ID_OFFSET]) << 8) | record[ID_OFFSET + 1]);
}

U16 getCrc(const U8* record) {
    return static_cast<U16>((static_cast<U16>(record[CRC_OFFSET]) << 8) | record[CRC_OFFSET + 1]);
}

bool crcValid(const U8* record) {
    return computeCrc(record) == getCrc(record);
}

bool isEmpty(const U8* record) {
    const U16 id = getId(record);
    return (id == EMPTY_ID) || (id == BLANK_ID);
}

void pack(U8* record, U16 id, const U8* value, FwSizeType length) {
    FW_ASSERT((length > 0) && (length <= VALUE_SIZE), static_cast<FwAssertArgType>(length));
    FW_ASSERT((id != EMPTY_ID) && (id != BLANK_ID), static_cast<FwAssertArgType>(id));
    record[ID_OFFSET] = static_cast<U8>(id >> 8);
    record[ID_OFFSET + 1] = static_cast<U8>(id);
    (void)::memcpy(&record[VALUE_OFFSET], value, static_cast<size_t>(length));
    (void)::memset(&record[VALUE_OFFSET + length], 0, static_cast<size_t>(VALUE_SIZE - length));
    const U16 crc = computeCrc(record);
    record[CRC_OFFSET] = static_cast<U8>(crc >> 8);
    record[CRC_OFFSET + 1] = static_cast<U8>(crc);
}

void packEmpty(U8* record) {
    (void)::memset(record, 0, static_cast<size_t>(SIZE));
}

}  // namespace PrmRecord
}  // namespace Samd21

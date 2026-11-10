// ======================================================================
// \title  PrmRecord.hpp
// \author tumbar
// \brief  Persistent record format of the static parameter database
// ======================================================================

#ifndef Samd21_PrmRecord_HPP
#define Samd21_PrmRecord_HPP

#include "Fw/FPrimeBasicTypes.hpp"
#include "default/samd-config/FppConstantsAc.hpp"

namespace Samd21 {

//! CRC-16/CCITT-FALSE polynomial (MSB first, initial value 0xFFFF, no final XOR)
constexpr U16 CRC16_POLY = 0x1021;

//! Initial value of a CRC-16/CCITT-FALSE
constexpr U16 CRC16_INIT = 0xFFFF;

//! Shift a single byte through the CRC-16 polynomial (bitwise, for compile-time use)
constexpr U16 crc16Byte(U16 crc, U8 byte) {
    crc = static_cast<U16>(crc ^ (static_cast<U16>(byte) << 8));
    for (FwIndexType b = 0; b < 8; b++) {
        crc = (crc & 0x8000) ? static_cast<U16>((crc << 1) ^ CRC16_POLY) : static_cast<U16>(crc << 1);
    }
    return crc;
}

//! CRC-16 over a buffer continuing from crc (table driven, for runtime use)
U16 crc16(U16 crc, const U8* data, FwSizeType len);

//! A persistent parameter record of VALUE_SIZE value bytes (StaticPrmDbConfig.RECORD_VALUE_SIZE):
//!
//!     [0..1]                 record ID, big endian
//!     [2..3]                 CRC-16 over the ID and the value bytes, big endian
//!     [4..4+VALUE_SIZE-1]    value bytes; a shorter segment is zero padded
//!
//! Records identify themselves, so they may sit anywhere in the store and in
//! any order. A parameter larger than VALUE_SIZE is split across several records.
namespace PrmRecord {

constexpr FwSizeType ID_OFFSET = 0;
constexpr FwSizeType ID_SIZE = sizeof(U16);
constexpr FwSizeType CRC_OFFSET = ID_OFFSET + ID_SIZE;
constexpr FwSizeType CRC_SIZE = sizeof(U16);
constexpr FwSizeType VALUE_OFFSET = CRC_OFFSET + CRC_SIZE;
constexpr FwSizeType VALUE_SIZE = StaticPrmDbConfig::RECORD_VALUE_SIZE;
constexpr FwSizeType SIZE = VALUE_OFFSET + VALUE_SIZE;

static_assert(VALUE_SIZE > 0, "StaticPrmDbConfig.RECORD_VALUE_SIZE must be at least 1");

//! Record ID of an unused slot (written as all zeros). Never assigned to a
//! parameter, and an EMPTY_ID or BLANK_ID slot is skipped without a CRC check.
constexpr U16 EMPTY_ID = 0x0000;

//! Record ID of erased (all 0xFF) storage. Never assigned to a parameter.
constexpr U16 BLANK_ID = 0xFFFF;

//! CRC of a record as stored (ID and the full 8 value bytes)
U16 computeCrc(const U8* record);

//! Record ID as stored
U16 getId(const U8* record);

//! CRC as stored
U16 getCrc(const U8* record);

//! True when the stored CRC matches the record contents
bool crcValid(const U8* record);

//! True when the record is an unused slot (EMPTY_ID or BLANK_ID)
bool isEmpty(const U8* record);

//! Fill a record with length (1..VALUE_SIZE) bytes of value, zero padded, and its CRC
void pack(U8* record, U16 id, const U8* value, FwSizeType length);

//! Fill a record with an unused slot
void packEmpty(U8* record);

}  // namespace PrmRecord
}  // namespace Samd21

#endif  // Samd21_PrmRecord_HPP

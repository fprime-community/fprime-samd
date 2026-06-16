// ======================================================================
// \title Samd21/Drv/RtcDriver/RawTime.cpp
// \brief Realtime clock implementation for Os::RawTime
// ======================================================================
#include "fprime-samd/Drv/RtcDriver/RawTime.hpp"
namespace Os {
namespace Samd21 {

//! \brief check is a is newer than b
bool isNewer(const Samd21RawTimeHandle& a, const Samd21RawTimeHandle& b) {
    if (a.m_seconds > b.m_seconds) {
        return true;
    } else if (a.m_seconds == b.m_seconds) {
        return a.m_micros > b.m_micros;
    } else {
        return false;
    }
}

RawTimeHandle* Samd21RawTime::getHandle() {
    return &this->m_handle;
}

RawTime::Status Samd21RawTime::now() {
    auto now = timeNow();
    this->m_handle = now;
    return Status::OP_OK;
}

RawTime::Status Samd21RawTime::getTimeInterval(const Os::RawTime& other, Fw::TimeInterval& interval) const {
    interval.set(0, 0);
    const Samd21RawTimeHandle& my_handle = this->m_handle;
    const Samd21RawTimeHandle& other_handle =
        static_cast<const Samd21RawTimeHandle&>(*const_cast<Os::RawTime&>(other).getHandle());

    const Samd21RawTimeHandle& newer = isNewer(my_handle, other_handle) ? my_handle : other_handle;
    const Samd21RawTimeHandle& older = isNewer(my_handle, other_handle) ? other_handle : my_handle;

    if (newer.m_micros < older.m_micros) {
        interval.set(newer.m_seconds - older.m_seconds - 1, 1000000 + newer.m_micros - older.m_micros);
    } else {
        interval.set(newer.m_seconds - older.m_seconds, newer.m_micros - older.m_micros);
    }

    return Status::OP_OK;
}

Fw::SerializeStatus Samd21RawTime::serializeTo(Fw::SerialBufferBase& buffer, Fw::Endianness mode) const {
    Fw::SerializeStatus status = Fw::SerializeStatus::FW_SERIALIZE_OK;
    status = buffer.serializeFrom(this->m_handle.m_seconds, mode);
    if (status == Fw::FW_SERIALIZE_OK) {
        status = buffer.serializeFrom(this->m_handle.m_micros, mode);
    }
    return status;
}

Fw::SerializeStatus Samd21RawTime::deserializeFrom(Fw::SerialBufferBase& buffer, Fw::Endianness mode) {
    Fw::SerializeStatus status = Fw::SerializeStatus::FW_SERIALIZE_OK;
    status = buffer.deserializeTo(this->m_handle.m_seconds, mode);
    if (status == Fw::FW_SERIALIZE_OK) {
        status = buffer.deserializeTo(this->m_handle.m_micros, mode);
    }
    return status;
}
}  // namespace Samd21
}  // namespace Os

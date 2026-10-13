// ======================================================================
// \title Samd21/Drv/RtcDriver/RawTime.cpp
// \brief Realtime clock implementation for Os::RawTime
// ======================================================================
#include "fprime-samd/Drv/RtcDriver/RawTime.hpp"
namespace Os {
namespace Samd21 {

RawTimeHandle* RawTime::getHandle() {
    return &this->m_handle;
}

RawTime::Status RawTime::now() {
    auto now = timeNow();
    this->m_handle = now;
    return Status::OP_OK;
}

RawTime::Status RawTime::getTimeInterval(const Os::RawTime& other, Fw::TimeInterval& interval) const {
    interval = timeInterval(this->m_handle, other.m_handle);
    return Status::OP_OK;
}

Fw::SerializeStatus RawTime::serializeTo(Fw::SerialBufferBase& buffer, Fw::Endianness mode) const {
    Fw::SerializeStatus status = Fw::SerializeStatus::FW_SERIALIZE_OK;
    status = buffer.serializeFrom(this->m_handle.m_seconds, mode);
    if (status == Fw::FW_SERIALIZE_OK) {
        status = buffer.serializeFrom(this->m_handle.m_micros, mode);
    }
    return status;
}

Fw::SerializeStatus RawTime::deserializeFrom(Fw::SerialBufferBase& buffer, Fw::Endianness mode) {
    Fw::SerializeStatus status = Fw::SerializeStatus::FW_SERIALIZE_OK;
    status = buffer.deserializeTo(this->m_handle.m_seconds, mode);
    if (status == Fw::FW_SERIALIZE_OK) {
        status = buffer.deserializeTo(this->m_handle.m_micros, mode);
    }
    return status;
}
}  // namespace Samd21
}  // namespace Os

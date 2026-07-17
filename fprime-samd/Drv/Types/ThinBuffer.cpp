// ======================================================================
// \title  ThinBuffer.cpp
// \author tumbar
// \brief  cpp file for ThinBuffer class
// ======================================================================

#include "fprime-samd/Drv/Types/ThinBuffer.hpp"

namespace Samd21 {
ThinBuffer::ThinBuffer() : m_data(nullptr), m_size(0), m_context(0) {}

ThinBuffer::ThinBuffer(const Fw::Buffer& fwBuffer)
    : m_data(fwBuffer.getData()), m_size(fwBuffer.getSize()), m_context(fwBuffer.getContext()) {}
ThinBuffer::ThinBuffer(U8* data, const U32 size) : m_data(data), m_size(size), m_context(0) {}

U8* ThinBuffer ::getData() const {
    return this->m_data;
}

U32 ThinBuffer ::getSize() const {
    return this->m_size;
}

Fw::Buffer ThinBuffer ::getBuffer() const {
    return Fw::Buffer(this->m_data, this->m_size, this->m_context);
}
}  // namespace Samd21

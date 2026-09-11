// ======================================================================
// \title  ThinBuffer.hpp
// \author tumbar
// \brief  hpp file for ThinBuffer class
// ======================================================================

#ifndef Samd21_ThinBuffer_HPP
#define Samd21_ThinBuffer_HPP

#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Types/BasicTypes.h"
#include "config/FwSizeTypeAliasAc.h"

namespace Samd21 {
//! A datatype holding the same information as Fw::Buffer without the large storage overhead
class ThinBuffer {
  public:
    ThinBuffer();
    explicit ThinBuffer(const Fw::Buffer& fwBuffer);
    //! Leaves context at 0 -- callers that need context-based slot identification
    //! (e.g. ThinBufferManager) must use the 3-arg constructor instead.
    explicit ThinBuffer(U8* data, U32 size);
    ThinBuffer(U8* data, U32 size, U32 context);
    U8* getData() const;
    U32 getSize() const;
    Fw::Buffer getBuffer() const;

  private:
    U8* m_data;
    FwSizeType m_size;
    U32 m_context;
};
}  // namespace Samd21

#endif

// ======================================================================
// \title  ThinBuffer.hpp
// \author tumbar
// \brief  hpp file for ThinBuffer class
// ======================================================================

#include "Fw/Buffer/Buffer.hpp"
#include "Fw/Types/BasicTypes.h"
#include "config/FwSizeTypeAliasAc.h"

namespace Samd21 {
//! A datatype holding the same information as Fw::Buffer without the large storage overhead
class ThinBuffer {
  public:
    ThinBuffer();
    explicit ThinBuffer(const Fw::Buffer& fwBuffer);
    U8* getData() const;
    U32 getSize() const;
    Fw::Buffer getBuffer() const;

  private:
    U8* m_data;
    FwSizeType m_size;
    U32 m_context;
};
}  // namespace Samd21

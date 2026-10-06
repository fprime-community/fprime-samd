// ======================================================================
// \title  SystemInfo.hpp
// \author tumbar
// \brief  hpp file for SystemInfo component implementation class
// ======================================================================

#ifndef Samd21_SystemInfo_HPP
#define Samd21_SystemInfo_HPP

#include "fprime-samd/Drv/SystemInfo/SystemInfoComponentAc.hpp"

namespace Samd21 {

class SystemInfo final : public SystemInfoComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SystemInfo object
    SystemInfo(const char* const compName  //!< The component name
    );

    //! Destroy SystemInfo object
    ~SystemInfo();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command EMIT_SYSTEM_INFO
    //!
    //! Emit the system information event
    void EMIT_SYSTEM_INFO_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                     U32 cmdSeq            //!< The command sequence number
                                     ) override;
};

}  // namespace Samd21

#endif

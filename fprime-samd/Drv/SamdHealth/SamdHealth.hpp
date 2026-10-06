// ======================================================================
// \title  SamdHealth.hpp
// \author tumbar
// \brief  hpp file for SamdHealth component implementation class
// ======================================================================

#ifndef Samd21_SamdHealth_HPP
#define Samd21_SamdHealth_HPP

#include "fprime-samd/Drv/SamdHealth/SamdHealthComponentAc.hpp"

namespace Samd21 {

class SamdHealth final : public SamdHealthComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SamdHealth object
    SamdHealth(const char* const compName  //!< The component name
    );

    //! Destroy SamdHealth object
    ~SamdHealth();

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

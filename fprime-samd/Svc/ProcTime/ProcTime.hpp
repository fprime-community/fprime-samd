// ======================================================================
// \title  ProcTime.hpp
// \author tumbar
// \brief  hpp file for ProcTime component implementation class
// ======================================================================

#ifndef Samd21_ProcTime_HPP
#define Samd21_ProcTime_HPP

#include "fprime-samd/Svc/ProcTime/ProcTimeComponentAc.hpp"

namespace Samd21 {

class ProcTime final : public ProcTimeComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct ProcTime object
    ProcTime(const char* const compName  //!< The component name
    );

    //! Destroy ProcTime object
    ~ProcTime();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for getTime
    //!
    //! Port to retrieve time
    void getTime_handler(FwIndexType portNum,  //!< The port number
                         Fw::Time& time        //!< Reference to Time object
                         ) override;
};

}  // namespace Samd21

#endif

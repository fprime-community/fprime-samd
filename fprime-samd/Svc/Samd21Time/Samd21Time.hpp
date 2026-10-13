// ======================================================================
// \title  Samd21Time.hpp
// \author tumbar
// \brief  hpp file for Samd21Time component implementation class
// ======================================================================

#ifndef Samd21_Samd21Time_HPP
#define Samd21_Samd21Time_HPP

#include "Os/RawTime.hpp"
#include "fprime-samd/Svc/Samd21Time/Samd21TimeComponentAc.hpp"

namespace Samd21 {

class Samd21Time final : public Samd21TimeComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Samd21Time object
    Samd21Time(const char* const compName  //!< The component name
    );

    //! Destroy Samd21Time object
    ~Samd21Time();

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

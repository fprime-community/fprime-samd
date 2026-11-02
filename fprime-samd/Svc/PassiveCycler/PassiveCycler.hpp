// ======================================================================
// \title  PassiveCycler.hpp
// \author tumbar
// \brief  hpp file for PassiveCycler component implementation class
// ======================================================================

#ifndef Svc_PassiveCycler_HPP
#define Svc_PassiveCycler_HPP

#include "fprime-samd/Svc/PassiveCycler/PassiveCyclerComponentAc.hpp"

namespace Svc {

class PassiveCycler final : public PassiveCyclerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PassiveCycler object
    PassiveCycler(const char* const compName  //!< The component name
    );

    //! Destroy PassiveCycler object
    ~PassiveCycler();

    //! Drive the output cycle: call every connected consumer, repeat while any reports work
    void cycle();

    //! One main-loop turn: cycle(), then sleep until the next interrupt unless one was taken
    //! during the cycle (Samd21::Idle), in which case the caller's loop cycles again
    void idle();
};

}  // namespace Svc

#endif

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

    //! Drive the output cycle
    void cycle();
};

}  // namespace Svc

#endif

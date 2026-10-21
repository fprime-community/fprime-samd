// ======================================================================
// \title  CyclerSchedAdapter.hpp
// \author lestarch-autobot
// \brief  hpp file for CyclerSchedAdapter component implementation class
// ======================================================================

#ifndef Samd21_CyclerSchedAdapter_HPP
#define Samd21_CyclerSchedAdapter_HPP

#include "fprime-samd/Svc/CyclerSchedAdapter/CyclerSchedAdapterComponentAc.hpp"

namespace Samd21 {

//! Drives one Svc.Sched consumer from Svc.PassiveCycler: forwards each activeIn turn to
//! schedOut with the context unchanged and always reports no further work (returns false).
//! Stateless; see docs/sdd.md for the no-re-poll constraint this places on the consumer.
class CyclerSchedAdapter final : public CyclerSchedAdapterComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct CyclerSchedAdapter object
    explicit CyclerSchedAdapter(const char* const compName  //!< The component name
    );

    //! Destroy CyclerSchedAdapter object
    ~CyclerSchedAdapter();

    CyclerSchedAdapter(const CyclerSchedAdapter&) = delete;
    CyclerSchedAdapter& operator=(const CyclerSchedAdapter&) = delete;
    CyclerSchedAdapter(CyclerSchedAdapter&&) = delete;
    CyclerSchedAdapter& operator=(CyclerSchedAdapter&&) = delete;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for activeIn
    //!
    //! Turn from Svc.PassiveCycler.cycleOut; always returns false (no work pending)
    bool activeIn_handler(FwIndexType portNum,  //!< The port number
                          U32 context           //!< The call order
                          ) override;
};

}  // namespace Samd21

#endif

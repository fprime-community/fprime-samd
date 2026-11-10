// ======================================================================
// \title  Clock.hpp
// \brief  Test harness time source of the StaticTopology
// ======================================================================

#ifndef StaticTopology_Clock_HPP
#define StaticTopology_Clock_HPP

#include "test/StaticTopology/Clock/ClockComponentAc.hpp"

namespace StaticTopology {

class Clock final : public ClockComponentBase {
  public:
    explicit Clock(const char* const compName);
    ~Clock();

    //! Time returned to every caller from now on
    void set(const Fw::Time& time);

  private:
    void timeGetPort_handler(FwIndexType portNum, Fw::Time& time) override;

    Fw::Time m_time;
};

}  // namespace StaticTopology

#endif

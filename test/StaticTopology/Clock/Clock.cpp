// ======================================================================
// \title  Clock.cpp
// \brief  Test harness time source of the StaticTopology
// ======================================================================

#include "test/StaticTopology/Clock/Clock.hpp"

namespace StaticTopology {

Clock::Clock(const char* const compName) : ClockComponentBase(compName), m_time() {}

Clock::~Clock() {}

void Clock::set(const Fw::Time& time) {
    this->m_time = time;
}

void Clock::timeGetPort_handler(FwIndexType portNum, Fw::Time& time) {
    time = this->m_time;
}

}  // namespace StaticTopology

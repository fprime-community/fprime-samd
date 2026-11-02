// ======================================================================
// \title  IdleStub.cpp
// \brief  Native stub of Samd21::Idle: same decision on a simulated interrupt flag
// ======================================================================

#include "fprime-samd/Drv/Types/Idle.hpp"

namespace Samd21 {

static IdleStub::State s_state = {};

IdleStub::State& IdleStub::getState() {
    return s_state;
}

void IdleStub::resetState() {
    s_state = IdleStub::State();
}

void Idle::arm() {
    s_state.arms++;
    s_state.interrupted = false;
}

void Idle::sleepUnlessInterrupted() {
    if (!s_state.interrupted) {
        s_state.sleeps++;
    } else {
        s_state.skips++;
    }
}

}  // namespace Samd21

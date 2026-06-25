// ======================================================================
// \title  PassiveCycler.cpp
// \author tumbar
// \brief  cpp file for PassiveCycler component implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveCycler/PassiveCycler.hpp"
#include "config/FwIndexTypeAliasAc.h"

namespace Svc {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PassiveCycler ::PassiveCycler(const char* const compName) : PassiveCyclerComponentBase(compName) {}

PassiveCycler ::~PassiveCycler() {}

void PassiveCycler ::cycle() {
    for (FwIndexType i = 0; i < PassiveCycler::NUM_CYCLEOUT_OUTPUT_PORTS; i++) {
        if (this->isConnected_cycleOut_OutputPort(i)) {
            this->cycleOut_out(i, i);
        }
    }
}

}  // namespace Svc

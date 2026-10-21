// ======================================================================
// \title  CyclerSchedAdapter.cpp
// \author lestarch-autobot
// \brief  cpp file for CyclerSchedAdapter component implementation class
// ======================================================================

#include "fprime-samd/Svc/CyclerSchedAdapter/CyclerSchedAdapter.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

CyclerSchedAdapter ::CyclerSchedAdapter(const char* const compName) : CyclerSchedAdapterComponentBase(compName) {}

CyclerSchedAdapter ::~CyclerSchedAdapter() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

bool CyclerSchedAdapter ::activeIn_handler(FwIndexType portNum, U32 context) {
    // The autocoded schedOut_out asserts if schedOut is unconnected (REQ-CSA-005); the
    // consumer runs to completion on this stack before the call returns (REQ-CSA-003).
    this->schedOut_out(0, context);
    // Never report pending work: the consumer is called again only when the cycler walks this
    // slot again (another slot reported work, or the next interrupt), never on our account.
    return false;
}

}  // namespace Samd21

// ======================================================================
// \title  FatalHandler.cpp
// \author root
// \brief  cpp file for FatalHandler component implementation class
// ======================================================================

#include "fprime-samd/Svc/FatalHandler/FatalHandler.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

FatalHandler ::FatalHandler(const char* const compName) : FatalHandlerComponentBase(compName) {}

FatalHandler ::~FatalHandler() {}

void FatalHandler ::FatalReceive_handler(const FwIndexType portNum, /*!< The port number*/
                                         FwEventIdType Id           /*!< The ID of the FATAL event*/
) {
    FW_ASSERT(0, static_cast<FwAssertArgType>(portNum), static_cast<FwAssertArgType>(Id));
}

}  // namespace Samd21

// ======================================================================
// \title  FatalHandler.hpp
// \author root
// \brief  hpp file for FatalHandler component implementation class
// ======================================================================

#ifndef Samd21_FatalHandler_HPP
#define Samd21_FatalHandler_HPP

#include "fprime-samd/Svc/FatalHandler/FatalHandlerComponentAc.hpp"

namespace Samd21 {

class FatalHandler final : public FatalHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FatalHandler object
    FatalHandler(const char* const compName  //!< The component name
    );

    //! Destroy FatalHandler object
    ~FatalHandler();

    private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for FatalReceive
    //!
    void FatalReceive_handler(const FwIndexType portNum, /*!< The port number*/
                              FwEventIdType Id           /*!< The ID of the FATAL event*/
    );
};

}  // namespace Samd21

#endif

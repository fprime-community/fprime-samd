// ======================================================================
// \title  FatalHandler.hpp
// \author tumbar
// \brief  hpp file for FatalHandler component implementation class
// ======================================================================

#ifndef Samd21_FatalHandler_HPP
#define Samd21_FatalHandler_HPP

#include "Fw/Types/Assert.hpp"
#include "fprime-samd/Svc/FatalHandler/FatalHandlerComponentAc.hpp"

namespace Samd21 {

class FatalHandler final : public FatalHandlerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct FatalHandler object and make it the instance assertions report through
    FatalHandler(const char* const compName  //!< The component name
    );

    //! Destroy FatalHandler object
    ~FatalHandler();

    // ----------------------------------------------------------------------
    // Assert hook entry points (Samd21_AssertHook_FatalHandler)
    // ----------------------------------------------------------------------

    //! Disable interrupts, read the time once, and serialize the AssertionFailed event
    //! packet into destBuffer. A second assertion raised while one is being reported
    //! resets the MCU.
    static void formatAssert(FILE_NAME_ARG file,
                             FwSizeType lineNo,
                             FwSizeType numArgs,
                             FwAssertArgType arg1,
                             FwAssertArgType arg2,
                             FwAssertArgType arg3,
                             FwAssertArgType arg4,
                             FwAssertArgType arg5,
                             FwAssertArgType arg6,
                             CHAR* destBuffer,
                             FwSizeType buffSize);

    //! Send the packet formatted by formatAssert() out of fatalOut
    //! FatalHandlerConfig::PACKET_REPEAT times
    static void sendAssert(const CHAR* msg);

    //! Reset the MCU
    static void terminate();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for user-defined typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for FatalReceive
    //!
    void FatalReceive_handler(const FwIndexType portNum, /*!< The port number*/
                              FwEventIdType Id           /*!< The ID of the FATAL event*/
                              ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! Set once an assertion is being reported, so an assert raised while
    //! reporting resets instead of recursing
    volatile bool m_assertReached;

    //! Size of the packet formatAssert() produced and sendAssert() has not yet sent; 0 when none
    volatile FwSizeType m_packetSize;
};

}  // namespace Samd21

#endif

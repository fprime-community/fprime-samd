// ======================================================================
// \title  SyncFramer.hpp
// \author tumbar
// \brief  hpp file for SyncFramer component implementation class
// ======================================================================

#ifndef Samd21_SyncFramer_HPP
#define Samd21_SyncFramer_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "fprime-samd/Svc/SyncFramer/SyncFramerComponentAc.hpp"

namespace Samd21 {

class SyncFramer final : public SyncFramerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SyncFramer object
    SyncFramer(const char* const compName  //!< The component name
    );

    //! Destroy SyncFramer object
    ~SyncFramer();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comPacketIn
    void comPacketIn_handler(FwIndexType portNum,  //!< The port number
                             Fw::ComBuffer& data,  //!< Buffer containing packet data
                             U32 context           //!< Call context value; meaning chosen by user
                             ) override;

    //! Handler implementation for drvConnected
    //!
    //! Signal from the driver that it is ready
    void drvConnected_handler(FwIndexType portNum  //!< The port number
                              ) override;

  public:
    void sendFatalPacket(Fw::ComBuffer& data);
};

}  // namespace Samd21

#endif

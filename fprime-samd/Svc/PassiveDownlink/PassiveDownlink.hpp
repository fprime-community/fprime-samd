// ======================================================================
// \title  PassiveDownlink.hpp
// \author tumbar
// \brief  hpp file for PassiveDownlink component implementation class
// ======================================================================

#ifndef Samd21_PassiveDownlink_HPP
#define Samd21_PassiveDownlink_HPP

#include "fprime-samd/Svc/PassiveDownlink/PassiveDownlinkComponentAc.hpp"

namespace Samd21 {

class PassiveDownlink final : public PassiveDownlinkComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PassiveDownlink object
    PassiveDownlink(const char* const compName  //!< The component name
    );

    //! Destroy PassiveDownlink object
    ~PassiveDownlink();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for LogRecv
    //!
    //! Port for receiving events
    void LogRecv_handler(FwIndexType portNum,              //!< The port number
                         FwEventIdType id,                 //!< Log ID
                         Fw::Time& timeTag,                //!< Time Tag
                         const Fw::LogSeverity& severity,  //!< The severity argument
                         Fw::LogBuffer& args               //!< Buffer containing serialized log entry
                         ) override;
};

}  // namespace Samd21

#endif

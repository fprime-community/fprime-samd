// ======================================================================
// \title  PassiveEventManager.hpp
// \author tumbar
// \brief  hpp file for PassiveEventManager component implementation class
// ======================================================================

#ifndef Samd21_PassiveEventManager_HPP
#define Samd21_PassiveEventManager_HPP

#include "fprime-samd/Svc/PassiveEventManager/PassiveEventManagerComponentAc.hpp"

namespace Samd21 {

class PassiveEventManager final : public PassiveEventManagerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct PassiveEventManager object
    PassiveEventManager(const char* const compName  //!< The component name
    );

    //! Destroy PassiveEventManager object
    ~PassiveEventManager();

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

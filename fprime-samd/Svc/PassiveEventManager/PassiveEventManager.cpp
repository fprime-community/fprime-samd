// ======================================================================
// \title  PassiveEventManager.cpp
// \author tumbar
// \brief  cpp file for PassiveEventManager component implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveEventManager/PassiveEventManager.hpp"
#include "Fw/Log/LogPacket.hpp"
#include "Fw/Types/Serializable.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PassiveEventManager ::PassiveEventManager(const char* const compName) : PassiveEventManagerComponentBase(compName) {}

PassiveEventManager ::~PassiveEventManager() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PassiveEventManager ::LogRecv_handler(FwIndexType portNum,
                                           FwEventIdType id,
                                           Fw::Time& timeTag,
                                           const Fw::LogSeverity& severity,
                                           Fw::LogBuffer& args) {
    Fw::LogPacket logPacket;
    Fw::ComBuffer buf;
    logPacket.setId(id);
    logPacket.setTimeTag(timeTag);
    logPacket.setLogBuffer(args);

    // A maximum-size LogBuffer plus the packet header can exceed the com buffer capacity.
    // Drop the packet rather than asserting; a FATAL is still announced below.
    const Fw::SerializeStatus stat = logPacket.serializeTo(buf);
    if ((Fw::FW_SERIALIZE_OK == stat) && this->isConnected_PktSend_OutputPort(0)) {
        this->PktSend_out(0, buf, 0);
    }

    // if connected, announce the FATAL
    if (Fw::LogSeverity::FATAL == severity.e) {
        if (this->isConnected_FatalAnnounce_OutputPort(0)) {
            this->FatalAnnounce_out(0, id);
        }
    }
}

}  // namespace Samd21

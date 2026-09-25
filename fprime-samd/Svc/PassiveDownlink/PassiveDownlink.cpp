// ======================================================================
// \title  PassiveDownlink.cpp
// \author tumbar
// \brief  cpp file for PassiveDownlink component implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveDownlink/PassiveDownlink.hpp"
#include "Fw/Log/LogPacket.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "config/FwAssertArgTypeAliasAc.h"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

PassiveDownlink ::PassiveDownlink(const char* const compName) : PassiveDownlinkComponentBase(compName) {}

PassiveDownlink ::~PassiveDownlink() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void PassiveDownlink ::LogRecv_handler(FwIndexType portNum,
                                       FwEventIdType id,
                                       Fw::Time& timeTag,
                                       const Fw::LogSeverity& severity,
                                       Fw::LogBuffer& args) {
    Fw::LogPacket logPacket;
    Fw::ComBuffer buf;
    logPacket.setId(id);
    logPacket.setTimeTag(timeTag);
    logPacket.setLogBuffer(args);

    Fw::SerializeStatus stat = logPacket.serializeTo(buf);
    FW_ASSERT(Fw::FW_SERIALIZE_OK == stat, static_cast<FwAssertArgType>(stat));

    if (this->isConnected_PktSend_OutputPort(0)) {
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

// ======================================================================
// \title  PassiveDownlink.cpp
// \author tumbar
// \brief  cpp file for PassiveDownlink component implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveDownlink/PassiveDownlink.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Svc/PassiveDownlink/PassiveDownlink_PacketPortEnumAc.hpp"

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
    this->m_logPacket.setId(id);
    this->m_logPacket.setTimeTag(timeTag);
    this->m_logPacket.setLogBuffer(args);
    this->m_logBuffer.resetSer();

    Fw::SerializeStatus stat = this->m_logPacket.serializeTo(this->m_logBuffer);
    FW_ASSERT(Fw::FW_SERIALIZE_OK == stat, static_cast<FwAssertArgType>(stat));

    if (this->isConnected_PktSend_OutputPort(PassiveDownlink_PacketPort::EVENT)) {
        this->PktSend_out(PassiveDownlink_PacketPort::EVENT, this->m_logBuffer, 0);
    }

    // if connected, announce the FATAL
    if (Fw::LogSeverity::FATAL == severity.e) {
        if (this->isConnected_FatalAnnounce_OutputPort(0)) {
            this->FatalAnnounce_out(0, id);
        }
    }
}

void PassiveDownlink ::TlmRecv_handler(FwIndexType portNum, FwChanIdType id, Fw::Time& timeTag, Fw::TlmBuffer& val) {
    // Attempt to serialize the telemetry value into the current buffer
    Fw::SerializeStatus status;
    status = this->m_tlmPacket.addValue(id, timeTag, val);

    if (status == Fw::SerializeStatus::FW_SERIALIZE_NO_ROOM_LEFT) {
        // We cannot fit the tlm value into this buffer
        // Flush the buffer and add it to a clean buffer
        if (this->isConnected_PktSend_OutputPort(PassiveDownlink_PacketPort::TELEMETRY)) {
            this->PktSend_out(PassiveDownlink_PacketPort::TELEMETRY, m_tlmPacket.getBuffer(), 0);
        }

        this->m_tlmPacket.resetPktSer();
        status = this->m_tlmPacket.addValue(id, timeTag, val);
        FW_ASSERT(status == Fw::SerializeStatus::FW_SERIALIZE_OK, static_cast<FwAssertArgType>(status));
    } else if (status == Fw::SerializeStatus::FW_SERIALIZE_OK) {
        // Telemetry value was successfully added to the buffer
    } else {
        FW_ASSERT(false, static_cast<FwAssertArgType>(status));
    }
}

}  // namespace Samd21

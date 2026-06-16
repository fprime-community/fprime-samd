// ======================================================================
// \title  PassiveDownlink.cpp
// \author tumbar
// \brief  cpp file for PassiveDownlink component implementation class
// ======================================================================

#include "fprime-samd/Svc/PassiveDownlink/PassiveDownlink.hpp"
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
    m_logPacket.setId(id);
    m_logPacket.setTimeTag(timeTag);
    m_logPacket.setLogBuffer(args);

    auto comBuffer = this->m_tlmPacket.getBuffer();
    comBuffer.resetSer();
    Fw::SerializeStatus stat = this->m_logPacket.serializeTo(comBuffer);
    FW_ASSERT(Fw::FW_SERIALIZE_OK == stat, static_cast<FwAssertArgType>(stat));

    if (this->isConnected_PktSend_OutputPort(PassiveDownlink_PacketPort::EVENT)) {
        this->PktSend_out(PassiveDownlink_PacketPort::EVENT, comBuffer, 0);
    }

    // if connected, announce the FATAL
    if (Fw::LogSeverity::FATAL == severity.e) {
        if (this->isConnected_FatalAnnounce_OutputPort(0)) {
            this->FatalAnnounce_out(0, id);
        }
    }
}

void PassiveDownlink ::TlmRecv_handler(FwIndexType portNum, FwChanIdType id, Fw::Time& timeTag, Fw::TlmBuffer& val) {
    m_tlmPacket.resetPktSer();

    Fw::SerializeStatus stat = m_tlmPacket.addValue(id, timeTag, val);
    FW_ASSERT(stat == Fw::FW_SERIALIZE_OK, stat);

    if (this->isConnected_PktSend_OutputPort(PassiveDownlink_PacketPort::TELEMETRY)) {
        this->PktSend_out(PassiveDownlink_PacketPort::TELEMETRY, m_tlmPacket.getBuffer(), 0);
    }
}

}  // namespace Samd21

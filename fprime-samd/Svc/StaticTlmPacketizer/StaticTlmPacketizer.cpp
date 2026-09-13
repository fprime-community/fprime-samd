// ======================================================================
// \title  StaticTlmPacketizer.cpp
// \author tumbar
// \brief  cpp file for StaticTlmPacketizer component implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticTlmPacketizer/StaticTlmPacketizer.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StaticTlmPacketizer ::StaticTlmPacketizer(const char* const compName) : StaticTlmPacketizerComponentBase(compName) {}

StaticTlmPacketizer ::~StaticTlmPacketizer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void StaticTlmPacketizer ::pktSendIn_handler(FwIndexType portNum, U32 context) {
    // TODO
}

void StaticTlmPacketizer ::tlmRecvIn_handler(FwIndexType portNum, FwChanIdType id, Fw::Time& timeTag, Fw::TlmBuffer& val) {
    this->writePoint(id, val);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticTlmPacketizer ::SEND_PKT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, U32 id) {
    this->sendPkt(id);
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

void StaticTlmPacketizer ::sendPkt(U32 id) {
    Fw::ComBuffer pkt;
    Fw::Time now = getTime();

    // Encode the ComPacket header
    auto status = pkt.serializeFrom(static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_PACKETIZED_TLM));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Encode TLM packet header
    status = pkt.serializeFrom(id);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    status = pkt.serializeFrom(now);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Load/copy the payload from the packet buffer into our packet buffer
    status = this->loadPacket(pkt, id);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Transmit the packet
    this->pktSendOut_out(0, pkt, 0);
}

}  // namespace Samd21

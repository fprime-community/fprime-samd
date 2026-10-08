// ======================================================================
// \title  StaticTlmPacketizer.cpp
// \author tumbar
// \brief  cpp file for StaticTlmPacketizer component implementation class
// ======================================================================

#include "fprime-samd/Svc/StaticTlmPacketizer/StaticTlmPacketizer.hpp"
#include "Fw/Cmd/CmdResponseEnumAc.hpp"
#include "Fw/Com/ComBuffer.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Types/Assert.hpp"
#include "Fw/Types/Serializable.hpp"
#include "Fw/Types/SuccessEnumAc.hpp"
#include "config/FwIndexTypeAliasAc.h"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

StaticTlmPacketizer::StaticTlmPacketizer(const char* const compName) : StaticTlmPacketizerComponentBase(compName) {}

StaticTlmPacketizer::~StaticTlmPacketizer() {}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void StaticTlmPacketizer::pktSendIn_handler(FwIndexType portNum, U32 context) {
    this->sendPkt(static_cast<FwTlmPacketizeIdType>(portNum));
}

void StaticTlmPacketizer::tlmRecvIn_handler(FwIndexType portNum,
                                            FwChanIdType id,
                                            Fw::Time& timeTag,
                                            Fw::TlmBuffer& val) {
    this->writePoint(id, val);
}

// ----------------------------------------------------------------------
// Handler implementations for commands
// ----------------------------------------------------------------------

void StaticTlmPacketizer::SEND_PKT_cmdHandler(FwOpcodeType opCode, U32 cmdSeq, FwTlmPacketizeIdType id) {
    auto status = this->sendPkt(id);
    this->cmdResponse_out(opCode, cmdSeq,
                          status == Fw::Success::SUCCESS ? Fw::CmdResponse::OK : Fw::CmdResponse::EXECUTION_ERROR);
}

Fw::Success StaticTlmPacketizer::sendPkt(FwTlmPacketizeIdType id) {
    Fw::ComBuffer pkt;
    Fw::Time now = getTime();

    // Encode the ComPacket header
    auto status = pkt.serializeFrom(static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_PACKETIZED_TLM));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Encode TLM packet header
    status = pkt.serializeFrom(static_cast<FwTlmPacketizeIdType>(id));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    status = pkt.serializeFrom(now);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

    // Load/copy the payload from the packet buffer into our packet buffer
    status = this->loadPacket(pkt, id);
    if (status == Fw::FW_SERIALIZE_FORMAT_ERROR) {
        // `id` comes from "user" input so we fail gracefully
        this->log_WARNING_LO_PacketNotFound(id);
        return Fw::Success::FAILURE;
    } else {
        // Make sure the packet fits in a ComBuffer
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);

        // Transmit to every connected receiver
        for (FwIndexType i = 0; i < NUM_PKTSENDOUT_OUTPUT_PORTS; i++) {
            if (this->isConnected_pktSendOut_OutputPort(i)) {
                this->pktSendOut_out(i, pkt, 0);
            }
        }

        return Fw::Success::SUCCESS;
    }
}

}  // namespace Samd21

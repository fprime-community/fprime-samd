// ======================================================================
// \title  Ground.cpp
// \brief  Test harness ground/sequencer stand-in of the StaticTopology
// ======================================================================

#include "test/StaticTopology/Ground/Ground.hpp"
#include "Fw/Com/ComPacket.hpp"
#include "Fw/Types/Assert.hpp"

namespace StaticTopology {

constexpr FwSizeType Ground::HISTORY;

Ground::Ground(const char* const compName)
    : GroundComponentBase(compName), m_responses(), m_responseCount(0), m_packets(), m_packetCount(0) {}

Ground::~Ground() {}

void Ground::sendCommandPacket(Fw::ComBuffer& packet, U32 context) {
    this->cmdBuffOut_out(0, packet, context);
}

void Ground::sendCommand(FwOpcodeType opCode, const U8* args, FwSizeType argsSize, U32 context) {
    Fw::ComBuffer packet;
    Fw::SerializeStatus status =
        packet.serializeFrom(static_cast<FwPacketDescriptorType>(Fw::ComPacketType::FW_PACKET_COMMAND));
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    status = packet.serializeFrom(opCode);
    FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    if (argsSize > 0) {
        status = packet.serializeFrom(args, argsSize, Fw::Serialization::OMIT_LENGTH);
        FW_ASSERT(status == Fw::FW_SERIALIZE_OK, status);
    }
    this->sendCommandPacket(packet, context);
}

void Ground::sendCmd(FwOpcodeType opCode, U32 context, Fw::CmdArgBuffer& args) {
    this->cmdOut_out(0, opCode, context, args);
}

void Ground::triggerPacket(FwIndexType portNum) {
    this->pktSendOut_out(portNum, 0);
}

void Ground::reset() {
    this->m_responseCount = 0;
    this->m_packetCount = 0;
}

FwSizeType Ground::getResponseCount() const {
    return this->m_responseCount;
}

const Ground::Response& Ground::getResponse(FwSizeType index) const {
    FW_ASSERT(index < this->m_responseCount && index < HISTORY, static_cast<FwAssertArgType>(index));
    return this->m_responses[index];
}

const Ground::Response& Ground::getLastResponse() const {
    FW_ASSERT(this->m_responseCount > 0);
    return this->m_responses[(this->m_responseCount < HISTORY ? this->m_responseCount : HISTORY) - 1];
}

FwSizeType Ground::getPacketCount() const {
    return this->m_packetCount;
}

const Ground::Packet& Ground::getPacket(FwSizeType index) const {
    FW_ASSERT(index < this->m_packetCount && index < HISTORY, static_cast<FwAssertArgType>(index));
    return this->m_packets[index];
}

const Ground::Packet& Ground::getLastPacket() const {
    FW_ASSERT(this->m_packetCount > 0);
    return this->m_packets[(this->m_packetCount < HISTORY ? this->m_packetCount : HISTORY) - 1];
}

void Ground::cmdStatusIn_handler(FwIndexType portNum,
                                 FwOpcodeType opCode,
                                 U32 cmdSeq,
                                 const Fw::CmdResponse& response) {
    FW_ASSERT(this->m_responseCount < HISTORY, static_cast<FwAssertArgType>(this->m_responseCount));
    this->m_responses[this->m_responseCount].opCode = opCode;
    this->m_responses[this->m_responseCount].context = cmdSeq;
    this->m_responses[this->m_responseCount].response = response;
    this->m_responseCount++;
}

void Ground::pktRecvIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) {
    FW_ASSERT(this->m_packetCount < HISTORY, static_cast<FwAssertArgType>(this->m_packetCount));
    this->m_packets[this->m_packetCount].port = portNum;
    this->m_packets[this->m_packetCount].data = data;
    this->m_packetCount++;
}

}  // namespace StaticTopology

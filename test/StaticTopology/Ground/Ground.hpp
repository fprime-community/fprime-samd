// ======================================================================
// \title  Ground.hpp
// \brief  Test harness ground/sequencer stand-in of the StaticTopology
// ======================================================================

#ifndef StaticTopology_Ground_HPP
#define StaticTopology_Ground_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "test/StaticTopology/Ground/GroundComponentAc.hpp"

namespace StaticTopology {

class Ground final : public GroundComponentBase {
  public:
    //! Responses and packets kept (oldest dropped first)
    static constexpr FwSizeType HISTORY = 16;

    struct Response {
        FwOpcodeType opCode;
        U32 context;
        Fw::CmdResponse response;
    };

    struct Packet {
        FwIndexType port;
        Fw::ComBuffer data;
    };

    explicit Ground(const char* const compName);
    ~Ground();

    //! Send a serialized command packet (descriptor, opcode, args) to the dispatcher
    void sendCommandPacket(Fw::ComBuffer& packet, U32 context);

    //! Send a command packet built from opcode and already-serialized args
    void sendCommand(FwOpcodeType opCode, const U8* args, FwSizeType argsSize, U32 context);

    //! Send a command through the dispatcher's Fw.Cmd sequencer port
    void sendCmd(FwOpcodeType opCode, U32 context, Fw::CmdArgBuffer& args);

    //! Trigger packet port portNum on the packetizer
    void triggerPacket(FwIndexType portNum);

    //! Clear the histories
    void reset();

    FwSizeType getResponseCount() const;
    const Response& getResponse(FwSizeType index) const;
    const Response& getLastResponse() const;

    FwSizeType getPacketCount() const;
    const Packet& getPacket(FwSizeType index) const;
    const Packet& getLastPacket() const;

  private:
    void cmdStatusIn_handler(FwIndexType portNum,
                             FwOpcodeType opCode,
                             U32 cmdSeq,
                             const Fw::CmdResponse& response) override;

    void pktRecvIn_handler(FwIndexType portNum, Fw::ComBuffer& data, U32 context) override;

    Response m_responses[HISTORY];
    FwSizeType m_responseCount;
    Packet m_packets[HISTORY];
    FwSizeType m_packetCount;
};

}  // namespace StaticTopology

#endif

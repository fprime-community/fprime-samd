// ======================================================================
// \title  StaticTlmPacketizer.hpp
// \author tumbar
// \brief  hpp file for StaticTlmPacketizer component implementation class
// ======================================================================

#ifndef Samd21_StaticTlmPacketizer_HPP
#define Samd21_StaticTlmPacketizer_HPP

#include "Fw/Types/Serializable.hpp"
#include "fprime-samd/Svc/StaticTlmPacketizer/StaticTlmPacketizerComponentAc.hpp"

namespace Samd21 {

class StaticTlmPacketizer final : public StaticTlmPacketizerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct StaticTlmPacketizer object
    StaticTlmPacketizer(const char* const compName  //!< The component name
    );

    //! Destroy StaticTlmPacketizer object
    ~StaticTlmPacketizer();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for pktSendIn
    //!
    //! Send a telemetry packet
    void pktSendIn_handler(FwIndexType portNum,  //!< The port number
                           U32 context           //!< The call order
                           ) override;

    //! Handler implementation for tlmRecvIn
    //!
    //! Telemetry input port
    void tlmRecvIn_handler(FwIndexType portNum,  //!< The port number
                           FwChanIdType id,      //!< Telemetry Channel ID
                           Fw::Time& timeTag,    //!< Time Tag
                           Fw::TlmBuffer& val    //!< Buffer containing serialized telemetry value
                           ) override;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for commands
    // ----------------------------------------------------------------------

    //! Handler implementation for command SEND_PKT
    //!
    //! Send a telemetry packet
    void SEND_PKT_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                             U32 cmdSeq,           //!< The command sequence number
                             U32 id                //!< The packet ID
                             ) override;

  private:
    //! Send a telemetry packet given it's id
    void sendPkt(U32 id);

    //! Given a telemetry id/value, write the value to the memory holding this value
    void writePoint(FwChanIdType id, const Fw::TlmBuffer& val);

    //! Load a telemetry packet into a ComBuffer given the packet's ID
    //! This function is autocoded at the topology level
    Fw::SerializeStatus loadPacket(Fw::ComBuffer& dest, U32 id);
};

}  // namespace Samd21

#endif

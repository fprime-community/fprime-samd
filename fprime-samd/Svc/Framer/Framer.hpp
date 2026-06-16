// ======================================================================
// \title  Framer.hpp
// \author tumbar
// \brief  hpp file for Framer component implementation class
// ======================================================================

#ifndef Samd21_Framer_HPP
#define Samd21_Framer_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "fprime-samd/Svc/Framer/FramerComponentAc.hpp"

namespace Samd21 {

class Framer final : public FramerComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct Framer object
    Framer(const char* const compName  //!< The component name
    );

    //! Destroy Framer object
    ~Framer();

    void sendFatalPacket(Fw::ComBuffer& data);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comPacketQueueIn
    //!
    //! Input port that handles downlink packets
    void comPacketQueueIn_handler(FwIndexType portNum,  //!< The port number
                                  Fw::ComBuffer& data,  //!< Buffer containing packet data
                                  U32 context           //!< Call context value; meaning chosen by user
                                  ) override;

    //! Handler implementation for drvConnected
    //!
    //! Signal from the driver that it is ready
    void drvConnected_handler(FwIndexType portNum  //!< The port number
                              ) override;

  private:
    bool m_driverConnected;
    Fw::ComBuffer m_packet;
};

}  // namespace Samd21

#endif

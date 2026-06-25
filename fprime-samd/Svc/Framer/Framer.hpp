// ======================================================================
// \title  Framer.hpp
// \author tumbar
// \brief  hpp file for Framer component implementation class
// ======================================================================

#ifndef Samd21_Framer_HPP
#define Samd21_Framer_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "fprime-samd/Svc/Framer/FramerComponentAc.hpp"
#include "fprime-samd/config/FramerConfig.hpp"

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

    //! Handler implementation for drvReturnIn
    //!
    //! Receive buffer back from driver
    void drvReturnIn_handler(FwIndexType portNum,  //!< The port number
                             Fw::Buffer& fwBuffer,
                             const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for schedIn
    //!
    //! Input port that flushes the active Tx buffer to the driver
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

  private:
    //! Flush the active buffer to the driver
    void flushActiveBuffer();

    //! Double-buffer state
    enum BufferState {
        IDLE,         //!< Buffer is idle and available for accumulation
        ACTIVE,       //!< Buffer is being filled with ComBuffers
        TRANSMITTING  //!< Buffer has been sent to driver
    };

    struct TxBuffer {
        U8 data[FramerConfig::FRAMER_TX_BUFFER_SIZE];  //!< Buffer storage
        FwSizeType size;                               //!< Current size of data in buffer
        BufferState state;                             //!< Current state of buffer
    };

    bool m_driverConnected;
    TxBuffer m_buffers[2];          //!< Double buffer
    FwIndexType m_activeBufferIdx;  //!< Index of currently active buffer
};

}  // namespace Samd21

#endif

// ======================================================================
// \title  UsartDriver.hpp
// \author tumbar
// \brief  hpp file for UsartDriver component implementation class
// ======================================================================

#ifndef Samd21_UsartDriver_HPP
#define Samd21_UsartDriver_HPP

#include "Fw/DataStructures/FifoQueue.hpp"
#include "config/FwSizeTypeAliasAc.h"
#include "config/UsartDriverConfig.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriverComponentAc.hpp"

namespace Samd21 {

class UsartDriver final : public UsartDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct UsartDriver object
    UsartDriver(const char* const compName  //!< The component name
    );

    //! Destroy UsartDriver object
    ~UsartDriver();

    enum class ClockMode : U8 {
        EXTERNAL = 0x0,
        INTERNAL = 0x1,
    };

    enum class CommunicationMode : U8 {
        ASYNC = 0x0,
        SYNC = 0x1,
    };

    // These bits define the receive data (RxD) pin configuration.
    enum class RxPinOut : U8 {
        PAD0 = 0,  //!< SERCOM PAD[0] is used for data reception
        PAD1 = 1,  //!< SERCOM PAD[1] is used for data reception
        PAD2 = 2,  //!< SERCOM PAD[2] is used for data reception
        PAD3 = 3,  //!< SERCOM PAD[3] is used for data reception
    };

    enum class TxPinOut : U8 {
        PAD0 = 0,  //!< TX: SERCOM PAD[0], XCK: SERCOM PAD[1]
        PAD1 = 1,  //!< TX: SERCOM PAD[2], XCK: SERCOM PAD[3]
        PAD2 = 2,  //!< TX: SERCOM PAD[0], RTS: SERCOM PAD[2], CTS: SERCOM PAD[3]
    };

    enum class DataOrder : U8 {
        MSB_FIRST = 0x0,  //!< MSB is transmitted first
        LSB_FIRST = 0x1,  //!< LSB is transmitted first
    };

    enum class Parity : U8 {
        NONE,
        EVEN,
        ODD,
    };

    enum class StopBits : U8 {
        ONE = 0x0,
        TWO = 0x1,
    };

    enum class DataBits : U8 {
        BITS_8 = 0x0,
        BITS_9 = 0x1,
        BITS_5 = 0x5,
        BITS_6 = 0x6,
        BITS_7 = 0x7,
    };

    enum class BaudRate : U32 {
        BAUD_9600 = 9600,
        BAUD_19200 = 19200,
        BAUD_38400 = 38400,
        BAUD_57600 = 57600,
        BAUD_115200 = 115200,  //!< Modern default
        BAUD_230400 = 230400,
        BAUD_460800 = 460800,
        BAUD_921600 = 921600,
    };

    void configure(SercomKind sercom,
                   RxPinOut rx,
                   TxPinOut tx,
                   ClockMode clock,
                   CommunicationMode mode,
                   BaudRate baud_rate,
                   DataOrder data_order,
                   DataBits data_bits,
                   StopBits stop_bits,
                   Parity parity,
                   FwSizeType rx_buffer_size,
                   U16 rx_dog_cnt);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Service the UART driver:
    //! 1. Return Tx buffers that finished DMA transmission.
    //! 2. Detect IDLE Rx. Pull the in-progress DMA Rx and send it downstream
    void schedIn_handler(FwIndexType portNum, U32 context) override;

    //! Handler implementation for dmaReplyIn
    //!
    //! A signal from the DMAC that a request has finished.
    //! This signal comes inside an ISR!
    void dmaReplyIn_handler(FwIndexType portNum,  //!< The port number
                            const Samd21::Dma::Reply& reply) override;

    //! Handler implementation for recvReturnIn
    //!
    //! Port receiving back ownership of data sent out on $recv port
    void recvReturnIn_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& fwBuffer  //!< The buffer
                              ) override;

    //! Handler implementation for send
    //!
    //! Invoke this port to send data out the driver (asynchronous)
    //! Status and ownership of the buffer are returned through the sendReturnOut callback
    //! send buffer to the DMAC synchronously
    void send_handler(FwIndexType portNum,  //!< The port number
                      Fw::Buffer& fwBuffer  //!< The buffer
                      ) override;

    //! Handler for input port cycleIn
    void cycleIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! An enum tracking the
    enum class RxDmaBufferID : U8 {
        A,
        B,
        INVALID,
    };

    enum class RxDmaBufferState : U8 {
        UNINITIALIZED,
        DMA,
        DMA_WAITING,
        IN_USE,
    };

    //! Calculate BAUD register value based on baud rate and mode
    U16 calculateBaud(BaudRate baud_rate, CommunicationMode mode);

    //! Get SERCOM TX DMA trigger source
    Dma::TriggerSource getSercomTxTrigger(SercomKind sercom);

    //! Get SERCOM RX DMA trigger source
    Dma::TriggerSource getSercomRxTrigger(SercomKind sercom);

    //! Handle a reply on the TX DMA channel
    void dmaReplyTxIsr(const Samd21::Dma::Reply& reply);

    //! Handle a reply on the RX DMA channel
    void dmaReplyRxIsr(const Samd21::Dma::Reply& reply);

    //! Send a buffer to the RX DMA channel
    void dmaQueueRxSend(const ThinBuffer& buffer);

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! SERCOM peripheral this driver controls
    SercomKind m_sercom;

    //! Fifo tracking the queue of TX DMA transactions.
    //! DMA replies should come back in the same order
    Fw::FifoQueue<ThinBuffer, UsartDriverConfig::TX_BUFFER_N> m_tx_queue;

    //! Rx buffers (A and B) for receiving data over the DMA
    ThinBuffer m_rx[2];
    RxDmaBufferState m_rx_state[2];
    RxDmaBufferID m_active_rx;

    //! Tracks whether configure() as been called or not
    bool m_configured;

    //! Watchdog counter for detecting IDLE Rx
    U16 m_rx_dog;

    //! Watchdog counter reset number
    U16 m_rx_dog_reset;

    //! Signals sent to the internal queue for processing during schedIn
    enum class SignalKind : U8 {
        TX_BUFFER_OK,      //!< A buffer has been TXed over the DMA successfully
        TX_CHANNEL_ERROR,  //!< An error occurred on the TX DMA channel. Clear all the TX buffers off the queue to try
                           //!< again
        RX_BUFFER_DONE,    //!< An RX buffer has been filled/partially filled and is ready for processing
        RX_CHANNEL_ERROR,  //!< An error occurred on the RX DMA channel.
    };

    struct Signal {
        SignalKind kind;
        RxDmaBufferID rx;
        U16 rx_bytes_remaining;
    };

    Fw::FifoQueue<Signal, 4> m_queue;
};

}  // namespace Samd21

#endif

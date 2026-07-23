// ======================================================================
// \title  UsartDriver.hpp
// \author tumbar
// \brief  hpp file for UsartDriver component implementation class
// ======================================================================

#ifndef Samd21_UsartDriver_HPP
#define Samd21_UsartDriver_HPP

#include "Fw/DataStructures/FifoQueue.hpp"
#include "config/UsartDriverConfig.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
#include "fprime-samd/Drv/UsartDriver/UsartDriverComponentAc.hpp"
#include "fprime-samd/config/UsartDriverConfig.hpp"

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

    // ----------------------------------------------------------------------
    // Component configuration
    // ----------------------------------------------------------------------

    enum class ClockMode : U8 {
        EXTERNAL = 0x0,  //!< Drive baud on the external XCK pad
        INTERNAL = 0x1,  //!< Drive the baud off a prescaled GCLK0
    };

    enum class CommunicationMode : U8 {
        ASYNC = 0x0,  //!< Asynchronous UART with TX/RX
        SYNC = 0x1,   //!< Synchronous UART with TX/RX, RTS/CTS
    };

    // These bits define the receive data (RxD) pin configuration.
    enum class RxPinOut : U8 {
        PAD0 = 0,  //!< SERCOM PAD[0] is used for data reception
        PAD1 = 1,  //!< SERCOM PAD[1] is used for data reception
        PAD2 = 2,  //!< SERCOM PAD[2] is used for data reception
        PAD3 = 3,  //!< SERCOM PAD[3] is used for data reception
    };

    enum class TxPinOut : U8 {
        PAD0 = 0,                    //!< TX: SERCOM PAD[0], XCK: SERCOM PAD[1]
        PAD2_XCK_PAD3 = 1,           //!< TX: SERCOM PAD[2], XCK: SERCOM PAD[3]
        PAD0_RTS_PAD2_CTS_PAD3 = 2,  //!< TX: SERCOM PAD[0], RTS: SERCOM PAD[2], CTS: SERCOM PAD[3]
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
                   Parity parity);

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

    //! Handler for input port activeIn
    void activeIn_handler(FwIndexType portNum,  //!< The port number
                          U32 context           //!< The call order
                          ) override;

    //! Handler for input port sendSync
    Drv::ByteStreamStatus sendSync_handler(FwIndexType portNum,    //!< The port number
                                           Fw::Buffer& sendBuffer  //!< Data to send
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
    Fw::FifoQueue<ThinBuffer, UsartDriverConfig::USART_TX_BUFFER_N> m_tx_queue;

    struct RxBuffer {
        U8 data[USART_RX_BUFFER_SIZE];
    };

    //! Rx buffers (A and B) for receiving data over the DMA
    RxBuffer m_rx[2];
    RxDmaBufferID m_active_rx;

    //! Tracks whether configure() as been called or not
    bool m_configured;

    U16 m_active_processed;

    //! m_rxOverflows: SERCOM STATUS.BUFOVF events (silent hardware RX drops).
    U32 m_rxOverflows;

    //! m_schedSkips: partial-read passes skipped by the high-RX watchdog.
    U32 m_schedSkips;

    //! High-RX watchdog. dmaReplyRxIsr bumps m_rx_activity on every RX
    //! completion (ISR context); schedIn compares it against the value seen on
    //! the previous tick. If it advanced, RX is actively moving full buffers, so
    //! schedIn skips the partial read -- which would suspend the RX DMA channel
    //! (see DmaChannel::readWriteback) and risk a BUFOVF drop. Full buffers are
    //! delivered losslessly via RX_BUFFER_DONE, so skipping loses nothing; the
    //! partial read only matters during genuine idle to extract a sub-buffer tail.
    volatile U32 m_rx_activity;
    U32 m_last_sched_activity;

    U32 m_rxBytes;
    U32 m_txBytes;

    //! Signals sent to the internal queue for processing during schedIn
    enum class SignalKind : U8 {
        TX_BUFFER_OK,       //!< A buffer has been TXed over the DMA successfully
        RX_BUFFER_PARTIAL,  //!< An RX buffer has been partially filled and is ready for processing
        RX_BUFFER_DONE,     //!< An RX buffer has been filled and is ready for processing
    };

    struct Signal {
        SignalKind kind;
        U16 rx_bytes;

        Signal() : kind(SignalKind::TX_BUFFER_OK), rx_bytes(0) {}
        explicit Signal(SignalKind kind_, U16 rx_bytes_) : kind(kind_), rx_bytes(rx_bytes_) {}
    };

    Fw::FifoQueue<Signal, UsartDriverConfig::USART_QUEUE_DEPTH> m_queue;
};

}  // namespace Samd21

#endif

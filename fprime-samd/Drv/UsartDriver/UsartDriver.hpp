// ======================================================================
// \title  UsartDriver.hpp
// \author tumbar
// \brief  hpp file for UsartDriver component implementation class
// ======================================================================

#ifndef Samd21_UsartDriver_HPP
#define Samd21_UsartDriver_HPP

#include "fprime-samd/Drv/Types/SercomEnumAc.hpp"
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
        MSB = 0x0,  //!< MSB is transmitted first
        LSB = 0x1,  //!< LSB is transmitted first
    };

    enum class Parity : U8 {
        NONE,
        EVEN,
        ODD,
    };

    enum class StopBits {
        ONE = 0x0,
        TWO = 0x1,
    };

    enum class DataBits {
        BITS_8 = 0x0,
        BITS_9 = 0x1,
        BITS_5 = 0x5,
        BITS_6 = 0x6,
        BITS_7 = 0x7,
    };

    enum class BaudRate : U32 {
        BAUD_9600 = 9600,      //!< Standard low-speed (default for many devices)
        BAUD_19200 = 19200,    //!< Common low-speed
        BAUD_38400 = 38400,    //!< Common medium-speed
        BAUD_57600 = 57600,    //!< Common medium-speed
        BAUD_115200 = 115200,  //!< Very common, modern default
        BAUD_230400 = 230400,  //!< High-speed
        BAUD_460800 = 460800,  //!< High-speed
        BAUD_921600 = 921600,  //!< Very high-speed
    };

    void configure(Sercom sercom,
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
};

}  // namespace Samd21

#endif

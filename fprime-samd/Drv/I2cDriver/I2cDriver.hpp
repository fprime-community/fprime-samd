// ======================================================================
// \title  I2cDriver.hpp
// \author tumbar
// \brief  hpp file for I2cDriver component implementation class
// ======================================================================

#ifndef Samd21_I2cDriver_HPP
#define Samd21_I2cDriver_HPP

#include "fprime-samd/Drv/I2cDriver/I2cDriverComponentAc.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"

namespace Samd21 {

void i2cDriverIsrHandler(SercomKind, void*);

class I2cDriver final : public I2cDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct I2cDriver object
    I2cDriver(const char* const compName  //!< The component name
    );

    //! Destroy I2cDriver object
    ~I2cDriver();

    //! This bit enables the SCL low time-out. If SCL is held low for 25ms-35ms, the host will release its
    //! clock hold, if enabled, and complete the current transaction. A stop condition will automatically be
    //! transmitted.
    enum class SclLowTimeout {
        DISABLED = 0,  //!< Time-out disabled
        ENABLED = 1,   //!< Time-out enabled
    };

    //! If the inactive bus time-out is enabled and the bus is inactive for longer than the time-out setting,
    //! the bus state logic will be set to idle. An inactive bus arise when either an I2C host or client is holding
    //! the SCL low.
    //! Enabling this option is necessary for SMBus compatibility, but can also be used in a non-SMBus
    //! set-up.
    //! Calculated time-out periods are based on a 100kHz baud rate.
    enum class InactiveTimeout {
        DISABLED = 0x0,        //!< Disabled
        TIMEOUT_55_US = 0x1,   //!< 5-6 SCL cycle time-out (50-60µs)
        TIMEOUT_105_US = 0x2,  //!< 10-11 SCL cycle time-out (100-110µs)
        TIMEOUT_205_US = 0x3,  //!< 20-21 SCL cycle time-out (200-210µs)
    };

    //! This bit controls when SCL will be stretched for software interaction.
    enum class ClockStretchMode {
        ALWAYS = 0x0,     //!< SCL stretch according to Figure 28-5
        AFTER_ACK = 0x1,  //!< SCL stretch only after ACK bit, Figure 28-6.
    };

    //! Target SCL bus frequency.
    //!
    //! The CTRLA.SPEED field only selects the electrical transfer *mode*
    //! (slew-rate / protocol timing); it does NOT set the SCL clock rate. The
    //! actual rate is derived from the GCLK_SERCOMx_CORE frequency and written
    //! to the BAUD register (§29.6.2.4.1). This enum couples the two together:
    //! from a single frequency the driver picks the correct CTRLA.SPEED mode
    //! (and forces CTRLA.SCLSM=1 for High-speed) AND computes the BAUD value.
    //!
    //! Values are the target SCL frequency in Hz.
    enum class Frequency : U32 {
        STANDARD_100KHZ = 100000,      //!< Standard-mode (Sm), CTRLA.SPEED=0x0
        FAST_400KHZ = 400000,          //!< Fast-mode (Fm), CTRLA.SPEED=0x0
        FAST_PLUS_1MHZ = 1000000,      //!< Fast-mode Plus (Fm+), CTRLA.SPEED=0x1
        HIGH_SPEED_3400KHZ = 3400000,  //!< High-speed (Hs), CTRLA.SPEED=0x2 + SCLSM=1
    };

    //! This bit enables the client SCL low extend time-out. If SCL is cumulatively held low for greater than
    //! 25ms from the initial START to a STOP, the host will release its clock hold if enabled, and complete
    //! the current transaction. A STOP will automatically be transmitted.
    //! SB or MB will be set as normal, but CLKHOLD will be release. The MEXTTOUT and BUSERR status bits
    //! will be set.
    enum class ClientSclLowTimeout {
        DISABLED = 0x0,  //!< Time-out disabled
        ENABLED = 0x1,   //!< Time-out enabled
    };

    //! This bit enables the host SCL low extend time-out. If SCL is cumulatively held low for greater than
    //! 10ms from START-to-ACK, ACK-to-ACK, or ACK-to-STOP the host will release its clock hold if enabled,
    //! and complete the current transaction. A STOP will automatically be transmitted.
    //! SB or MB will be set as normal, but CLKHOLD will be released. The MEXTTOUT and BUSERR status
    //! bits will be set.
    enum class HostSclLowTimeout {
        DISABLED = 0x0,  //!< Time-out disabled
        ENABLED = 0x1,   //!< Time-out enabled
    };

    //! These bits define the SDA hold time with respect to the negative edge of SCL.
    enum class SdaHold {
        DISABLED = 0x0,
        HOLD_75_NS = 0x1,   //!< 50-100ns hold time
        HOLD_450_NS = 0x2,  //!< 300-600ns hold time
        HOLD_600_NS = 0x3,  //!< 400-800ns hold time
    };

    //! This bit set the pin usage to either two- or four-wire operation:
    enum class PinUsage {
        TWO_WIRE = 0x0,   //!< SCL/SDA. 4-wire operation disabled.
        FOUR_WIRE = 0x1,  //!< SCL/SDA/SCL_OUT/SDA_OUT. 4-wire operation enabled.
    };

    //! This bit defines the functionality in standby sleep mode.
    enum class RunInStandby {
        DISABLED,  //!< GCLK_SERCOMx_CORE is disabled and the I2C host will not operate in standby sleep mode.
        ENABLED,   //!< GCLK_SERCOMx_CORE is enabled in all sleep modes.
    };

    //! Configure SERCOM device for I2C Host (master) mode
    void configure(SercomKind sercom,
                   SclLowTimeout scl_low_timeout,
                   InactiveTimeout inactive_timeout,
                   ClockStretchMode clock_stretch_mode,
                   Frequency frequency,
                   ClientSclLowTimeout client_scl_low_timeout,
                   HostSclLowTimeout host_scl_low_timeout,
                   SdaHold sda_hold,
                   PinUsage pin_usage,
                   RunInStandby run_in_standby);

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

    //! Handler implementation for read
    //!
    //! Port for asynchronous read transaction
    void read_handler(FwIndexType portNum,  //!< The port number
                      U32 addr,             //!< I2C slave device address
                      Fw::Buffer& buffer    //!< Buffer with data to write or space for read data
                      ) override;

    //! Handler implementation for write
    //!
    //! Port for asynchronous write transaction
    void write_handler(FwIndexType portNum,  //!< The port number
                       U32 addr,             //!< I2C slave device address
                       Fw::Buffer& buffer    //!< Buffer with data to write or space for read data
                       ) override;

    //! Handler implementation for writeRead
    //!
    //! Port for asynchronous write-read transaction
    void writeRead_handler(FwIndexType portNum,      //!< The port number
                           U32 addr,                 //!< I2C slave device address
                           Fw::Buffer& writeBuffer,  //!< Buffer to write
                           Fw::Buffer& readBuffer    //!< Buffer to read into
                           ) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! SERCOM peripheral this driver controls
    SercomKind m_sercom;

    //! Tracks whether configure() has been called or not
    bool m_configured;

    //! Current operation executing on the I2C/DMA
    enum class State : U8 {
        IDLE,
        READ,
        WRITE,
        WRITE_READ_WRITING,
        WRITE_READ_READING,
    };

    //! Current peripheral state/executing transaction
    State m_state;

    //! Current/pending read buffer
    Samd21::ThinBuffer m_read;

    //! The address that is pending during a writeRead operation
    U32 m_pending_read_address;

    //! Current/finished write buffer
    Samd21::ThinBuffer m_write;

    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    // Allow the isr handler call into a private function in this class
    friend void Samd21::i2cDriverIsrHandler(SercomKind, void*);
    void isrHandler();
};

}  // namespace Samd21

#endif

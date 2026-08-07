// ======================================================================
// \title  I2cDriverHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for I2C driver peripheral operations
//
// All raw SERCOM I2CM register access, clock/NVIC setup and ISR registration
// live behind this interface so the I2cDriver component logic (state machine,
// error dispatch, telemetry mapping, port replies) can be exercised natively
// against a stub. Mirrors the Rtc/Usart driver HAL split.
// ======================================================================

#ifndef Samd21_I2cDriverHardware_HPP
#define Samd21_I2cDriverHardware_HPP

#include "Fw/Comp/PassiveComponentBase.hpp"
#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"

namespace Samd21 {
namespace I2cHardware {

//! Snapshot of the interrupt-relevant SERCOM I2CM flags, read once at the top
//! of the ISR. Booleans decode the raw INTFLAG/STATUS register bits so the
//! component's ISR logic never touches hardware directly.
struct I2cInterruptStatus {
    bool error;             //!< INTFLAG.ERROR: an error interrupt is pending
    bool masterOnBus;       //!< INTFLAG.MB: master-on-bus interrupt is pending
    bool busError;          //!< STATUS.BUSERR
    bool arbLost;           //!< STATUS.ARBLOST
    bool lowTimeout;        //!< STATUS.LOWTOUT
    bool masterExtTimeout;  //!< STATUS.MEXTTOUT
    bool slaveExtTimeout;   //!< STATUS.SEXTTOUT
    bool lengthError;       //!< STATUS.LENERR
    bool rxNack;            //!< STATUS.RXNACK: the client NACKed the last address/data byte
};

//! Snapshot of the SERCOM I2CM status used to build the periodic telemetry.
struct I2cBusStatus {
    U8 busState;       //!< STATUS.BUSSTATE (2-bit): 0=UNKNOWN 1=IDLE 2=OWNER 3=BUSY
    bool clockHold;    //!< STATUS.CLKHOLD
    bool rxNack;       //!< STATUS.RXNACK
    bool slaveOnBus;   //!< INTFLAG.SB
    bool masterOnBus;  //!< INTFLAG.MB
};

//! Hardware abstraction layer for the SERCOM I2C host peripheral.
struct I2cHal {
    //! Register the component's ISR trampoline with the SERCOM dispatch table.
    //!
    //! On the SAMD21 target this forwards to SercomUtil::registerIsrHandler; the
    //! stub simply records the callback so tests can fire it synchronously.
    static void registerIsr(SercomKind sercom,
                            void (*callback)(Fw::PassiveComponentBase*, SercomKind),
                            Fw::PassiveComponentBase* data);

    //! Configure and enable the SERCOM peripheral for I2C host operation.
    //!
    //! Performs the full §29.6.2.1 initialization sequence (clock gating, GCLK
    //! routing, CTRLA/CTRLB, baud rate, NVIC) and returns once the peripheral is
    //! enabled and its bus state forced to IDLE. All sync waits are bounded.
    static void configure(SercomKind sercom,
                          I2cDriver::SclLowTimeout scl_low_timeout,
                          I2cDriver::InactiveTimeout inactive_timeout,
                          I2cDriver::ClockStretchMode clock_stretch_mode,
                          I2cDriver::Frequency frequency,
                          I2cDriver::ClientSclLowTimeout client_scl_low_timeout,
                          I2cDriver::HostSclLowTimeout host_scl_low_timeout,
                          I2cDriver::SdaHold sda_hold,
                          I2cDriver::PinUsage pin_usage,
                          I2cDriver::RunInStandby run_in_standby);

    //! Address of the SERCOM I2CM DATA register (DMA source for RX, dest for TX).
    static U32 getDataRegisterAddress(SercomKind sercom);

    //! Read the interrupt flags/status for the ISR (INTFLAG.ERROR/MB + STATUS errors).
    static I2cInterruptStatus readInterruptStatus(SercomKind sercom);

    //! Read the bus status for periodic telemetry.
    static I2cBusStatus readBusStatus(SercomKind sercom);

    //! Acknowledge (write-1-clear) all error STATUS bits and INTFLAG.ERROR.
    static void acknowledgeErrors(SercomKind sercom);

    //! Acknowledge (write-1-clear) the master-on-bus interrupt (INTFLAG.MB).
    static void acknowledgeMasterOnBus(SercomKind sercom);

    //! Enable the master-on-bus interrupt (INTENSET.MB).
    static void enableMasterOnBusInterrupt(SercomKind sercom);

    //! Disable the master-on-bus interrupt (INTENCLR.MB).
    static void disableMasterOnBusInterrupt(SercomKind sercom);

    //! Kick off a read transaction: set the read ACK action and write ADDR with
    //! length mode enabled so the DMA drives the transfer (§29.6.2.4).
    //! \param addr 7-bit slave address
    //! \param byteCount number of bytes to read (ADDR.LEN)
    static void beginRead(SercomKind sercom, U32 addr, U8 byteCount);

    //! Kick off a write transaction: set the write ACK action and write ADDR.
    //! \param addr 7-bit slave address
    //! \param byteCount number of bytes to write (ADDR.LEN when stop generated)
    //! \param generateStopCondition when true, enable ADDR.LENEN so the peripheral
    //!        emits an automatic STOP after byteCount bytes; when false the clock is
    //!        held for a repeated START (write-read).
    static void beginWrite(SercomKind sercom, U32 addr, U8 byteCount, bool generateStopCondition);
};

//! Test-only hooks into the stub hardware implementation.
//! Compiled only for native/test builds (not the SAMD21 target); lets unit
//! tests observe HAL arguments, inject interrupt/bus status, and fire the ISR.
#ifndef __SAMD21__
struct StubState {
    // configure() capture
    bool configured;
    U32 configure_count;
    SercomKind sercom;
    I2cDriver::SclLowTimeout scl_low_timeout;
    I2cDriver::InactiveTimeout inactive_timeout;
    I2cDriver::ClockStretchMode clock_stretch_mode;
    I2cDriver::Frequency frequency;
    I2cDriver::ClientSclLowTimeout client_scl_low_timeout;
    I2cDriver::HostSclLowTimeout host_scl_low_timeout;
    I2cDriver::SdaHold sda_hold;
    I2cDriver::PinUsage pin_usage;
    I2cDriver::RunInStandby run_in_standby;

    // registerIsr() capture
    void (*isr_callback)(Fw::PassiveComponentBase*, SercomKind);
    Fw::PassiveComponentBase* isr_data;
    SercomKind isr_sercom;

    // getDataRegisterAddress() control
    U32 data_register_address;

    // readInterruptStatus()/readBusStatus() injected values
    I2cInterruptStatus interrupt_status;
    I2cBusStatus bus_status;

    // ack / interrupt-enable counters
    U32 acknowledge_errors_count;
    U32 acknowledge_mb_count;
    U32 enable_mb_count;
    U32 disable_mb_count;

    // beginRead() capture
    U32 begin_read_count;
    U32 begin_read_addr;
    U8 begin_read_len;

    // beginWrite() capture
    U32 begin_write_count;
    U32 begin_write_addr;
    U8 begin_write_len;
    bool begin_write_stop;
};

//! Get the mutable stub state (shared across all HAL calls)
StubState& getStubState();

//! Reset the stub state for a clean test run
void resetStubState();

//! Fire the ISR callback registered via registerIsr(), as the hardware would.
void fireIsr();
#endif

}  // namespace I2cHardware
}  // namespace Samd21

#endif

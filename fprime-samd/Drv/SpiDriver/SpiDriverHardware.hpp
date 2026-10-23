// ======================================================================
// \title  SpiDriverHardware.hpp
// \brief  Hardware abstraction layer for the SERCOM SPI host peripheral
//
// All raw SERCOM SPI register access (clock gating, GCLK routing, CTRLA/CTRLB,
// BAUD, enable) lives behind this interface so the SpiDriver component logic
// (busy state, chip select, DMA sequencing, replies) can be exercised natively
// against a stub. Mirrors the I2c/Usart driver HAL split.
// ======================================================================
#ifndef Samd21_SpiDriverHardware_HPP
#define Samd21_SpiDriverHardware_HPP

#include "Fw/Comp/PassiveComponentBase.hpp"
#include "fprime-samd/Drv/SpiDriver/SpiDriver.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"

namespace Samd21 {
namespace SpiHardware {

//! SERCOM SPI error state as seen from the ERROR interrupt.
struct SpiErrorStatus {
    bool error;           //!< INTFLAG.ERROR: an error interrupt is pending
    bool bufferOverflow;  //!< STATUS.BUFOVF: a received character was lost
};

//! Hardware abstraction layer for the SERCOM SPI host peripheral.
struct SpiHal {
    //! Register the SERCOM interrupt callback for this driver instance.
    //! Must be called before configure(), which enables the ERROR interrupt.
    static void registerIsr(SercomKind sercom,
                            void (*callback)(Fw::PassiveComponentBase*, SercomKind),
                            Fw::PassiveComponentBase* data);

    //! Configure and enable the SERCOM peripheral for SPI host operation.
    //!
    //! Performs the §27.6.2.1 initialization sequence (clock gating, GCLK
    //! routing, software reset, CTRLA/CTRLB, BAUD, enable). All sync waits are
    //! bounded. Asserts on an unknown SERCOM or an unachievable baud rate.
    static void configure(SercomKind sercom,
                          U32 baud_rate_khz,
                          SpiDriver::DataOrder data_order,
                          SpiDriver::ClockPolarity clock_polarity,
                          SpiDriver::ClockPhase clock_phase,
                          SpiDriver::DataInPinout data_in_pinout,
                          SpiDriver::DataOutPinout data_out_pinout,
                          SpiDriver::RunInStandby run_in_standby,
                          SpiDriver::HardwareChipSelect hardware_chipselect);

    //! Change SCK rate after configure(): disables the SERCOM (BAUD is enable-protected,
    //! §27.6.2.1), writes BAUD and re-enables it. Only legal while no transfer is in flight.
    static void setBaudRate(SercomKind sercom, U32 baud_rate_khz);

    //! Address of the SERCOM SPI DATA register (DMA source for MISO, destination for MOSI).
    static U32 getDataRegisterAddress(SercomKind sercom);

    //! Read the error interrupt state (INTFLAG.ERROR, STATUS.BUFOVF). ISR context.
    static SpiErrorStatus readErrorStatus(SercomKind sercom);

    //! Acknowledge an error after the DMA streams have been stopped: wait (bounded) for
    //! the shifter to go idle, write-1-clear STATUS.BUFOVF, drain any character left in
    //! DATA and clear INTFLAG.ERROR so the interrupt does not re-fire. ISR context.
    static void acknowledgeError(SercomKind sercom);
};

//! Test-only hooks into the stub hardware implementation.
//! Compiled only for native/test builds (not the SAMD21 target); lets unit
//! tests observe HAL arguments and choose the DATA register address.
#ifndef __SAMD21__
struct StubState {
    // configure() capture
    bool configured;
    U32 configure_count;
    SercomKind sercom;
    U32 baud_rate_khz;
    SpiDriver::DataOrder data_order;
    SpiDriver::ClockPolarity clock_polarity;
    SpiDriver::ClockPhase clock_phase;
    SpiDriver::DataInPinout data_in_pinout;
    SpiDriver::DataOutPinout data_out_pinout;
    SpiDriver::RunInStandby run_in_standby;
    SpiDriver::HardwareChipSelect hardware_chipselect;

    // getDataRegisterAddress() control
    U32 data_register_address;

    // registerIsr() capture
    void (*isr_callback)(Fw::PassiveComponentBase*, SercomKind);
    Fw::PassiveComponentBase* isr_data;
    SercomKind isr_sercom;

    // readErrorStatus() injected value
    SpiErrorStatus error_status;

    // acknowledgeError() capture
    U32 acknowledge_error_count;
    U32 set_baud_count;
};

//! Invoke the registered ISR callback as the SERCOM interrupt would
void fireIsr();

//! Get the mutable stub state (shared across all HAL calls)
StubState& getStubState();

//! Reset the stub state for a clean test run
void resetStubState();
#endif

}  // namespace SpiHardware
}  // namespace Samd21

#endif

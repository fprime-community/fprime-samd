// ======================================================================
// \title  GpioDriverHardware.hpp
// \author tumbar
// \brief  Hardware abstraction layer for GPIO driver peripheral operations
// ======================================================================

#ifndef Samd21_GpioDriverHardware_HPP
#define Samd21_GpioDriverHardware_HPP

#include "Fw/Types/LogicEnumAc.hpp"
#include "fprime-samd/Drv/GpioDriver/GpioDriver.hpp"

namespace Samd21 {
namespace GpioHardware {

//! Hardware abstraction layer for GPIO peripheral operations
struct GpioHal {
    //! Configure GPIO pin in input mode
    static void configureInput(U8 groupIdx, U8 pinIdx, GpioDriver::InputPullMode input_pull_mode);

    //! Configure GPIO pin in output mode
    static void configureOutput(U8 groupIdx, U8 pinIdx);

    //! Configure the External Interrupt Controller for edge detection on an input pin.
    //!
    //! The pin must already be configured as an input (see configureInput). Routes
    //! the pin to its EIC EXTINT line and enables interrupt generation for the
    //! selected edge(s). interrupt_mode must not be NONE.
    static void configureExternalInterrupt(U8 groupIdx,
                                           U8 pinIdx,
                                           GpioDriver::ExternalInterruptMode interrupt_mode);

    //! Read the logic level on a given input pin
    static Fw::Logic read(U8 groupIdx, U8 pinIdx);

    //! Write the logic level to a given output pin
    static void write(U8 groupIdx, U8 pinIdx, const Fw::Logic& state);

    //! Read the EIC interrupt flag bitmask (bit x set => EXTINT[x] is pending)
    static U32 getInterruptFlags();

    //! Acknowledge the given EIC interrupt flags (write-1-to-clear)
    static void clearInterruptFlags(U32 mask);
};

//! Register a component to be notified when its pin's external interrupt fires.
//!
//! The single EIC_Handler ISR dispatches to the component registered for the
//! pin's EXTINT line. Called during input configuration for pins with an
//! interrupt mode other than NONE.
//! \param pinIdx Pin index within the group; selects EXTINT line = pinIdx % 16
//! \param handler Component whose gpioInterruptIsr() is invoked on an edge
void registerInterruptHandler(U8 pinIdx, GpioDriver* handler);

//! Get the component registered for an EXTINT line, or nullptr if none.
//! \param extintLine EXTINT line index [0..15]
GpioDriver* getInterruptHandler(U8 extintLine);

//! Test helper functions (only available in stub implementation)
#ifndef __SAMD21__

//! Observable state recorded by the stub HAL for unit testing
struct GpioState {
    //! Number of times configureInput() was called
    U32 configure_input_count;
    //! Number of times configureExternalInterrupt() was called
    U32 configure_external_interrupt_count;
    //! Number of times configureOutput() was called
    U32 configure_output_count;
    //! Number of times write() was called
    U32 write_count;
    //! Number of times read() was called
    U32 read_count;

    //! Arguments captured from the most recent configureInput()/configureOutput() call
    U8 last_group;
    U8 last_pin;
    //! Pull mode captured from the most recent configureInput() call
    GpioDriver::InputPullMode last_input_pull_mode;
    //! Interrupt mode captured from the most recent configureInput() call
    GpioDriver::ExternalInterruptMode last_input_interrupt_mode;

    //! Arguments captured from the most recent write() call
    U8 last_write_group;
    U8 last_write_pin;
    Fw::Logic last_write_state;

    //! Arguments captured from the most recent read() call
    U8 last_read_group;
    U8 last_read_pin;

    //! Value returned by the next read() call
    Fw::Logic read_value;
};

//! Get the global stub GPIO state
//! \return Reference to global GPIO state
GpioState& getGpioState();

//! Reset stub GPIO state for clean test runs
void resetGpioState();

//! Set the value that read() will return
//! \param state Logic level to return on the next read()
void setReadValue(const Fw::Logic& state);

//! Simulate an external interrupt edge on a pin, dispatching to its registered
//! handler exactly as the EIC_Handler ISR would on hardware.
//! \param pinIdx Pin index within the group; selects EXTINT line = pinIdx % 16
void simulateInterrupt(U8 pinIdx);

#endif

}  // namespace GpioHardware
}  // namespace Samd21

#endif

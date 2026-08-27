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
    static void configureInput(U8 groupIdx,
                               U8 pinIdx,
                               GpioDriver::InputPullMode input_pull_mode,
                               GpioDriver::ExternalInterruptMode interrupt_mode);

    //! Configure GPIO pin in output mode
    static void configureOutput(U8 groupIdx, U8 pinIdx);

    //! Read the logic level on a given input pin
    static Fw::Logic read(U8 groupIdx, U8 pinIdx);

    //! Write the logic level to a given output pin
    static void write(U8 groupIdx, U8 pinIdx, const Fw::Logic& state);
};

//! Test helper functions (only available in stub implementation)
#ifndef __SAMD21__

//! Observable state recorded by the stub HAL for unit testing
struct GpioState {
    //! Number of times configureInput() was called
    U32 configure_input_count;
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

#endif

}  // namespace GpioHardware
}  // namespace Samd21

#endif

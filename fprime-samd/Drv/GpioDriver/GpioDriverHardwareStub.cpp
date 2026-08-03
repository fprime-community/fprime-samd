// ======================================================================
// \title  GpioDriverHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for GPIO peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, GpioDriverHardware.cpp is used instead.
//
// The stub records every HAL interaction in an observable GpioState so that
// unit tests can verify the driver drives the HAL correctly, and lets tests
// inject the value returned by read().
// ======================================================================

#include "Fw/Types/LogicEnumAc.hpp"
#include "fprime-samd/Drv/GpioDriver/GpioDriverHardware.hpp"

namespace Samd21 {
namespace GpioHardware {

//! Global GPIO state instance for stub
static GpioState g_gpio_state = {
    .configure_count = 0,
    .write_count = 0,
    .read_count = 0,
    .last_group = 0,
    .last_pin = 0,
    .last_mode = GpioDriver::Mode::INPUT,
    .last_input_pull_mode = GpioDriver::InputPullMode::NO_PULL,
    .last_write_group = 0,
    .last_write_pin = 0,
    .last_write_state = Fw::Logic::LOW,
    .last_read_group = 0,
    .last_read_pin = 0,
    .read_value = Fw::Logic::HIGH,
};

GpioState& getGpioState() {
    return g_gpio_state;
}

void GpioHal::configure(U8 groupIdx, U8 pinIdx, GpioDriver::Mode mode, GpioDriver::InputPullMode input_pull_mode) {
    g_gpio_state.configure_count++;
    g_gpio_state.last_group = groupIdx;
    g_gpio_state.last_pin = pinIdx;
    g_gpio_state.last_mode = mode;
    g_gpio_state.last_input_pull_mode = input_pull_mode;
}

Fw::Logic GpioHal::read(U8 groupIdx, U8 pinIdx) {
    g_gpio_state.read_count++;
    g_gpio_state.last_read_group = groupIdx;
    g_gpio_state.last_read_pin = pinIdx;
    return g_gpio_state.read_value;
}

void GpioHal::write(U8 groupIdx, U8 pinIdx, const Fw::Logic& state) {
    g_gpio_state.write_count++;
    g_gpio_state.last_write_group = groupIdx;
    g_gpio_state.last_write_pin = pinIdx;
    g_gpio_state.last_write_state = state;
}

//! Test helper: reset stub state for clean test runs
void resetGpioState() {
    g_gpio_state.configure_count = 0;
    g_gpio_state.write_count = 0;
    g_gpio_state.read_count = 0;
    g_gpio_state.last_group = 0;
    g_gpio_state.last_pin = 0;
    g_gpio_state.last_mode = GpioDriver::Mode::INPUT;
    g_gpio_state.last_input_pull_mode = GpioDriver::InputPullMode::NO_PULL;
    g_gpio_state.last_write_group = 0;
    g_gpio_state.last_write_pin = 0;
    g_gpio_state.last_write_state = Fw::Logic::LOW;
    g_gpio_state.last_read_group = 0;
    g_gpio_state.last_read_pin = 0;
    g_gpio_state.read_value = Fw::Logic::HIGH;
}

//! Test helper: set the value returned by the next read()
void setReadValue(const Fw::Logic& state) {
    g_gpio_state.read_value = state;
}

}  // namespace GpioHardware
}  // namespace Samd21

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

//! Number of EIC external interrupt lines (EXTINT[0..15]); a pin maps to line pinIdx % this.
static constexpr U8 EXTINT_LINE_COUNT = 16;

//! Global GPIO state instance for stub
static GpioState g_gpio_state = {
    .configure_input_count = 0,
    .configure_external_interrupt_count = 0,
    .configure_output_count = 0,
    .write_count = 0,
    .read_count = 0,
    .last_group = 0,
    .last_pin = 0,
    .last_input_pull_mode = GpioDriver::InputPullMode::NO_PULL,
    .last_input_interrupt_mode = GpioDriver::ExternalInterruptMode::NONE,
    .last_write_group = 0,
    .last_write_pin = 0,
    .last_write_state = Fw::Logic::LOW,
    .last_read_group = 0,
    .last_read_pin = 0,
    .read_value = Fw::Logic::HIGH,
};

//! Components registered for ISR dispatch, indexed by EXTINT line.
static GpioDriver* g_interrupt_handlers[EXTINT_LINE_COUNT] = {};

GpioState& getGpioState() {
    return g_gpio_state;
}

void GpioHal::configureInput(U8 groupIdx, U8 pinIdx, GpioDriver::InputPullMode input_pull_mode) {
    g_gpio_state.configure_input_count++;
    g_gpio_state.last_group = groupIdx;
    g_gpio_state.last_pin = pinIdx;
    g_gpio_state.last_input_pull_mode = input_pull_mode;
}

void GpioHal::configureExternalInterrupt(U8 groupIdx, U8 pinIdx, GpioDriver::ExternalInterruptMode interrupt_mode) {
    g_gpio_state.configure_external_interrupt_count++;
    g_gpio_state.last_group = groupIdx;
    g_gpio_state.last_pin = pinIdx;
    g_gpio_state.last_input_interrupt_mode = interrupt_mode;
}

U32 GpioHal::getInterruptFlags() {
    return 0;
}

void GpioHal::clearInterruptFlags(U32 mask) {
    (void)mask;
}

void registerInterruptHandler(U8 pinIdx, GpioDriver* handler) {
    FW_ASSERT(handler != nullptr);
    g_interrupt_handlers[pinIdx % EXTINT_LINE_COUNT] = handler;
}

GpioDriver* getInterruptHandler(U8 extintLine) {
    return g_interrupt_handlers[extintLine % EXTINT_LINE_COUNT];
}

void GpioHal::configureOutput(U8 groupIdx, U8 pinIdx) {
    g_gpio_state.configure_output_count++;
    g_gpio_state.last_group = groupIdx;
    g_gpio_state.last_pin = pinIdx;
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
    g_gpio_state.configure_input_count = 0;
    g_gpio_state.configure_external_interrupt_count = 0;
    g_gpio_state.configure_output_count = 0;
    g_gpio_state.write_count = 0;
    g_gpio_state.read_count = 0;
    g_gpio_state.last_group = 0;
    g_gpio_state.last_pin = 0;
    g_gpio_state.last_input_pull_mode = GpioDriver::InputPullMode::NO_PULL;
    g_gpio_state.last_input_interrupt_mode = GpioDriver::ExternalInterruptMode::NONE;
    g_gpio_state.last_write_group = 0;
    g_gpio_state.last_write_pin = 0;
    g_gpio_state.last_write_state = Fw::Logic::LOW;
    g_gpio_state.last_read_group = 0;
    g_gpio_state.last_read_pin = 0;
    g_gpio_state.read_value = Fw::Logic::HIGH;

    for (U8 line = 0; line < EXTINT_LINE_COUNT; line++) {
        g_interrupt_handlers[line] = nullptr;
    }
}

//! Test helper: set the value returned by the next read()
void setReadValue(const Fw::Logic& state) {
    g_gpio_state.read_value = state;
}

//! Test helper: dispatch an edge to a pin's registered handler like EIC_Handler would.
void simulateInterrupt(U8 pinIdx) {
    GpioDriver* handler = g_interrupt_handlers[pinIdx % EXTINT_LINE_COUNT];
    if (handler != nullptr) {
        handler->gpioInterruptIsr();
    }
}

}  // namespace GpioHardware
}  // namespace Samd21

// ======================================================================
// \title  GpioDriver.cpp
// \author tumbar
// \brief  cpp file for GpioDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/GpioDriver.hpp"
#include "fprime-samd/Drv/GpioDriver/GpioDriverHardware.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

GpioDriver ::GpioDriver(const char* const compName)
    : GpioDriverComponentBase(compName),
      m_configured(false),
      m_mode(Mode::INPUT),
      m_group(Group::PA),
      m_pin(Pin::PIN_0) {}

GpioDriver ::~GpioDriver() {}

void GpioDriver ::configureInput(Group group,
                                 Pin pin,
                                 InputPullMode input_pull_mode,
                                 ExternalInterruptMode interrupt_mode) {
    FW_ASSERT(!this->m_configured);

    this->m_group = group;
    this->m_pin = pin;
    this->m_mode = Mode::INPUT;

    const U8 groupIdx = static_cast<U8>(group);
    const U8 pinIdx = static_cast<U8>(pin);
    GpioHardware::GpioHal::configureInput(groupIdx, pinIdx, input_pull_mode);

    // Register for ISR dispatch before enabling the interrupt, so EIC_Handler
    // never sees the interrupt live with no handler registered for this line.
    if (interrupt_mode != ExternalInterruptMode::NONE) {
        GpioHardware::registerInterruptHandler(pinIdx, this);
        GpioHardware::GpioHal::configureExternalInterrupt(groupIdx, pinIdx, interrupt_mode);
    }

    this->m_configured = true;
}

void GpioDriver ::configureOutput(Group group, Pin pin) {
    FW_ASSERT(!this->m_configured);

    this->m_group = group;
    this->m_pin = pin;
    this->m_mode = Mode::OUTPUT;

    const U8 groupIdx = static_cast<U8>(group);
    const U8 pinIdx = static_cast<U8>(pin);
    GpioHardware::GpioHal::configureOutput(groupIdx, pinIdx);

    this->m_configured = true;
}

void GpioDriver ::gpioInterruptIsr() {
    // Nothing to emit if the interrupt notification port is not wired up.
    if (!this->isConnected_gpioInterrupt_OutputPort(0)) {
        return;
    }

    // Timestamp the edge and emit it as a cycle to the connected consumer.
    Os::RawTime cycleStart;
    (void)cycleStart.now();
    this->gpioInterrupt_out(0, cycleStart);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::GpioStatus GpioDriver ::gpioRead_handler(FwIndexType portNum, Fw::Logic& state) {
    if (!this->m_configured) {
        return Drv::GpioStatus::NOT_OPENED;
    }

    if (this->m_mode != Mode::INPUT) {
        return Drv::GpioStatus::INVALID_MODE;
    }

    state = GpioHardware::GpioHal::read(static_cast<U8>(this->m_group), static_cast<U8>(this->m_pin));
    return Drv::GpioStatus::OP_OK;
}

Drv::GpioStatus GpioDriver ::gpioWrite_handler(FwIndexType portNum, const Fw::Logic& state) {
    if (!this->m_configured) {
        return Drv::GpioStatus::NOT_OPENED;
    }

    if (this->m_mode != Mode::OUTPUT) {
        return Drv::GpioStatus::INVALID_MODE;
    }

    GpioHardware::GpioHal::write(static_cast<U8>(this->m_group), static_cast<U8>(this->m_pin), state);
    return Drv::GpioStatus::OP_OK;
}

}  // namespace Samd21

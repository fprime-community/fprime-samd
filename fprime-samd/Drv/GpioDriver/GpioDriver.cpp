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

void GpioDriver ::configure(Group group, Pin pin, Mode mode, InputPullMode input_pull_mode) {
    FW_ASSERT(!this->m_configured);

    this->m_group = group;
    this->m_pin = pin;
    this->m_mode = mode;

    const U8 groupIdx = static_cast<U8>(group);
    const U8 pinIdx = static_cast<U8>(pin);
    GpioHardware::GpioHal::configure(groupIdx, pinIdx, mode, input_pull_mode);

    this->m_configured = true;
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

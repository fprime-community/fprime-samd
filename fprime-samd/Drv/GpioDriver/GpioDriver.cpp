// ======================================================================
// \title  GpioDriver.cpp
// \author tumbar
// \brief  cpp file for GpioDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/GpioDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "samd.h"

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

void GpioDriver ::configure(Group group, Pin pin, Mode mode, bool enable_pull, InputPullMode input_pull_mode) {
    FW_ASSERT(!this->m_configured);

    this->m_group = group;
    this->m_pin = pin;
    this->m_mode = mode;

    const U8 groupIdx = static_cast<U8>(group);
    const U8 pinIdx = static_cast<U8>(pin);
    const U32 pinMask = static_cast<U32>(1) << pinIdx;

    PortGroup& portGroup = PORT->Group[groupIdx];

    uint8_t pinCfg = static_cast<uint8_t>(PORT_PINCFG_INEN);

    if (mode == Mode::OUTPUT) {
        portGroup.PINCFG[pinIdx].reg = pinCfg;
        portGroup.OUTCLR.reg = pinMask;
        portGroup.DIRSET.reg = pinMask;
    } else {
        // Configure as an input.
        portGroup.DIRCLR.reg = pinMask;

        if (enable_pull) {
            // With PULLEN set, the OUT register bit selects the pull direction:
            // OUT=1 -> pull-up, OUT=0 -> pull-down. Set the direction before
            // enabling the pull so the pad never briefly pulls the wrong way.
            if (input_pull_mode == InputPullMode::PULL_UP) {
                portGroup.OUTSET.reg = pinMask;
            } else {
                portGroup.OUTCLR.reg = pinMask;
            }
            pinCfg |= static_cast<uint8_t>(PORT_PINCFG_PULLEN);
        }
        portGroup.PINCFG[pinIdx].reg = pinCfg;
    }

    this->m_configured = true;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Drv::GpioStatus GpioDriver ::gpioRead_handler(FwIndexType portNum, Fw::Logic& state) {
    if (!this->m_configured) {
        return Drv::GpioStatus::NOT_OPENED;
    }

    const U32 pinMask = static_cast<U32>(1) << static_cast<U8>(this->m_pin);
    const PortGroup& portGroup = PORT->Group[static_cast<U8>(this->m_group)];
    const bool high = (portGroup.IN.reg & pinMask) != 0;

    if (high) {
        state = Fw::Logic::HIGH;
    } else {
        state = Fw::Logic::LOW;
    }

    return Drv::GpioStatus::OP_OK;
}

Drv::GpioStatus GpioDriver ::gpioWrite_handler(FwIndexType portNum, const Fw::Logic& state) {
    if (!this->m_configured) {
        return Drv::GpioStatus::NOT_OPENED;
    }

    if (this->m_mode != Mode::OUTPUT) {
        return Drv::GpioStatus::INVALID_MODE;
    }

    const U32 pinMask = static_cast<U32>(1) << static_cast<U8>(this->m_pin);
    PortGroup& portGroup = PORT->Group[static_cast<U8>(this->m_group)];
    if (state == Fw::Logic::HIGH) {
        portGroup.OUTSET.reg = pinMask;
    } else {
        portGroup.OUTCLR.reg = pinMask;
    }

    return Drv::GpioStatus::OP_OK;
}

}  // namespace Samd21

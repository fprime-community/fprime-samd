// ======================================================================
// \title  RtcDriverHardware.cpp
// \author tumbar
// \brief  MCU-specific hardware implementation for RTC peripheral
//
// This file is only compiled for SAMD21 target builds.
// For Linux/test builds, RtcDriverHardwareStub.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/GpioDriverHardware.hpp"
#include "Fw/Types/LogicEnumAc.hpp"
#include "samd.h"

namespace Samd21 {
namespace GpioHardware {

void GpioHal::configure(U8 groupIdx, U8 pinIdx, GpioDriver::Mode mode, GpioDriver::InputPullMode input_pull_mode) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    PortGroup& portGroup = PORT->Group[groupIdx];

    U8 pinCfg = static_cast<U8>(PORT_PINCFG_INEN);

    if (mode == GpioDriver::Mode::OUTPUT) {
        portGroup.PINCFG[pinIdx].reg = pinCfg;
        portGroup.OUTCLR.reg = pinMask;
        portGroup.DIRSET.reg = pinMask;
    } else {
        // Configure as an input.
        portGroup.DIRCLR.reg = pinMask;
        portGroup.PINCFG[pinIdx].reg = pinCfg;

        switch (input_pull_mode) {
            case GpioDriver::InputPullMode::NO_PULL:
                // No pull up/down resistors are connected
                break;
                // With PULLEN set, the OUT register bit selects the pull direction:
                // OUT=1 -> pull-up, OUT=0 -> pull-down. Set the direction before
                // enabling the pull so the pad never briefly pulls the wrong way.
            case GpioDriver::InputPullMode::PULL_DOWN:
                portGroup.OUTCLR.reg = pinMask;
                pinCfg |= static_cast<U8>(PORT_PINCFG_PULLEN);
                break;
            case GpioDriver::InputPullMode::PULL_UP:
                portGroup.OUTSET.reg = pinMask;
                pinCfg |= static_cast<U8>(PORT_PINCFG_PULLEN);
                break;
        }
    }
}

Fw::Logic GpioHal::read(U8 groupIdx, U8 pinIdx) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    const PortGroup& portGroup = PORT->Group[groupIdx];
    const bool isHigh = (portGroup.IN.reg & pinMask) != 0;

    return isHigh ? Fw::Logic::HIGH : Fw::Logic::LOW;
}

void GpioHal::write(U8 groupIdx, U8 pinIdx, const Fw::Logic& state) {
    const U32 pinMask = static_cast<U32>(1) << pinIdx;
    PortGroup& portGroup = PORT->Group[groupIdx];
    if (state == Fw::Logic::HIGH) {
        portGroup.OUTSET.reg = pinMask;
    } else {
        portGroup.OUTCLR.reg = pinMask;
    }
}

}  // namespace GpioHardware
}  // namespace Samd21

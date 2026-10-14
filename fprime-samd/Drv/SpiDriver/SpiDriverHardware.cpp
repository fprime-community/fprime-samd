// ======================================================================
// \title  SpiDriverHardware.cpp
// \brief  SAMD21 hardware implementation of the SERCOM SPI host HAL
// ======================================================================
#include "fprime-samd/Drv/SpiDriver/SpiDriverHardware.hpp"

#include <samd21/include/samd21g17a.h>

#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/Types/Sercom.hpp"

namespace Samd21 {
namespace SpiHardware {

// Bound hardware synchronization waits to ~1s (F_CPU cycles), matching the
// pattern used in RtcDriver/DmaDriver/UsartDriver/I2cDriver. A stuck sync flag
// asserts rather than hanging the CPU forever.
static void waitForGclkSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForSpiSync(Sercom* sercom_hw, U32 mask) {
    volatile U32 limit = F_CPU;
    while (limit > 0 && (sercom_hw->SPI.SYNCBUSY.reg & mask)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

void SpiHal::configure(SercomKind sercom,
                       U32 baud_rate_khz,
                       SpiDriver::DataOrder data_order,
                       SpiDriver::ClockPolarity clock_polarity,
                       SpiDriver::ClockPhase clock_phase,
                       SpiDriver::DataInPinout data_in_pinout,
                       SpiDriver::DataOutPinout data_out_pinout,
                       SpiDriver::RunInStandby run_in_standby,
                       SpiDriver::HardwareChipSelect hardware_chipselect) {
    // Get SERCOM hardware register base
    auto* sercom_hw = SercomUtil::getHardware(sercom);
    FW_ASSERT(sercom_hw != nullptr, sercom.e);

    // Enable SERCOM peripheral clock (APBC bus)
    // Per §16.8.7 PM – Power Manager APBC Mask
    switch (sercom.e) {
        case SercomKind::SERCOM_0:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM0;
            break;
        case SercomKind::SERCOM_1:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM1;
            break;
        case SercomKind::SERCOM_2:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM2;
            break;
        case SercomKind::SERCOM_3:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3;
            break;
#ifdef SERCOM4
        case SercomKind::SERCOM_4:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM4;
            break;
#endif
#ifdef SERCOM5
        case SercomKind::SERCOM_5:
            PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;
            break;
#endif
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(sercom));
    }

    waitForGclkSync();

    // Assign Generic Clock Generator 0 (48MHz) to the SERCOM core clock.
    // GCLK_SERCOMx_CORE clocks the SPI host baud-rate generator (§27.5.3).
    // Per §14.8.3 GCLK_CLKCTRL – Generic Clock Control
    U8 gclk_id = static_cast<U8>(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val) + sercom.e;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id) |
                        GCLK_CLKCTRL_GEN_GCLK0 |  // Use Generic Clock Generator 0 (48MHz main clock)
                        GCLK_CLKCTRL_CLKEN;
    waitForGclkSync();

    // Build CTRLA and CTRLB per the §27.6.2.1 initialization sequence. These
    // registers are enable-protected: they can only be written while
    // CTRLA.ENABLE=0.
    SERCOM_SPI_CTRLA_Type ctrla = {.reg = 0};
    SERCOM_SPI_CTRLB_Type ctrlb = {.reg = 0};

    // Select SPI Host (master) operating mode (CTRLA.MODE = 0x3)
    ctrla.bit.MODE = SERCOM_SPI_CTRLA_MODE_SPI_MASTER_Val;

    // Transfer mode: clock polarity (CTRLA.CPOL) and clock phase (CTRLA.CPHA)
    switch (clock_polarity) {
        case SpiDriver::ClockPolarity::IdleLow:
            ctrla.bit.CPOL = 0;
            break;
        case SpiDriver::ClockPolarity::IdleHigh:
            ctrla.bit.CPOL = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(clock_polarity));
    }

    switch (clock_phase) {
        case SpiDriver::ClockPhase::SampleOnLeadingSck:
            ctrla.bit.CPHA = 0;
            break;
        case SpiDriver::ClockPhase::SampleOnRisingSck:
            ctrla.bit.CPHA = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(clock_phase));
    }

    // Frame format: plain SPI frame (CTRLA.FORM = 0x0). Address-recognition
    // frames (0x2) are only meaningful in client mode.
    ctrla.bit.FORM = 0x0;

    // Data In Pinout: SERCOM pad used for MISO (CTRLA.DIPO)
    switch (data_in_pinout) {
        case SpiDriver::DataInPinout::PAD0:
            ctrla.bit.DIPO = 0x0;
            break;
        case SpiDriver::DataInPinout::PAD1:
            ctrla.bit.DIPO = 0x1;
            break;
        case SpiDriver::DataInPinout::PAD2:
            ctrla.bit.DIPO = 0x2;
            break;
        case SpiDriver::DataInPinout::PAD3:
            ctrla.bit.DIPO = 0x3;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_in_pinout));
    }

    // Data Out Pinout: pad layout for MOSI/SCK/SS (CTRLA.DOPO)
    switch (data_out_pinout) {
        case SpiDriver::DataOutPinout::MOSI_0_SCK_1_CS_2:
            ctrla.bit.DOPO = 0x0;
            break;
        case SpiDriver::DataOutPinout::MOSI_2_SCK_3_CS_1:
            ctrla.bit.DOPO = 0x1;
            break;
        case SpiDriver::DataOutPinout::MOSI_3_SCK_1_CS_2:
            ctrla.bit.DOPO = 0x2;
            break;
        case SpiDriver::DataOutPinout::MOSI_0_SCK_3_CS_1:
            ctrla.bit.DOPO = 0x3;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_out_pinout));
    }

    // Data order (CTRLA.DORD)
    switch (data_order) {
        case SpiDriver::DataOrder::MSB:
            ctrla.bit.DORD = 0;
            break;
        case SpiDriver::DataOrder::LSB:
            ctrla.bit.DORD = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_order));
    }

    // Run in standby (CTRLA.RUNSTDBY)
    switch (run_in_standby) {
        case SpiDriver::RunInStandby::DISABLED:
            ctrla.bit.RUNSTDBY = 0;
            break;
        case SpiDriver::RunInStandby::ENABLED:
            ctrla.bit.RUNSTDBY = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(run_in_standby));
    }

    // Character size: 8-bit characters (CTRLB.CHSIZE = 0x0)
    ctrlb.bit.CHSIZE = 0x0;

    // Enable the receiver so read data is shifted into DATA (CTRLB.RXEN)
    ctrlb.bit.RXEN = 1;

    // Hardware SS control (CTRLB.MSSEN). When enabled the SERCOM drives the
    // SS/chip-select pad selected by DOPO; otherwise the application drives a
    // GPIO.
    switch (hardware_chipselect) {
        case SpiDriver::HardwareChipSelect::DISABLED:
            ctrlb.bit.MSSEN = 0;
            break;
        case SpiDriver::HardwareChipSelect::ENABLED:
            ctrlb.bit.MSSEN = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(hardware_chipselect));
    }

    // §27.6.2.1 Initialization: enable-protected registers require ENABLE=0.

    // Reset the peripheral to a known state before configuring.
    sercom_hw->SPI.CTRLA.bit.ENABLE = 0;
    waitForSpiSync(sercom_hw, SERCOM_SPI_SYNCBUSY_ENABLE);

    sercom_hw->SPI.CTRLA.reg |= SERCOM_SPI_CTRLA_SWRST;
    waitForSpiSync(sercom_hw, SERCOM_SPI_SYNCBUSY_SWRST);

    // Write CTRLA (mode, transfer mode, pinouts, data order, standby)
    sercom_hw->SPI.CTRLA.reg = ctrla.reg;

    // Write CTRLB (character size, RX enable, hardware SS). CTRLB is
    // synchronized.
    sercom_hw->SPI.CTRLB.reg = ctrlb.reg;
    waitForSpiSync(sercom_hw, SERCOM_SPI_SYNCBUSY_CTRLB);

    // Program the baud rate to generate the target SCK.
    sercom_hw->SPI.BAUD.reg = SERCOM_SPI_BAUD_BAUD(SpiDriver::calculateBaud(F_CPU, baud_rate_khz));

    // Enable the peripheral.
    sercom_hw->SPI.CTRLA.reg |= SERCOM_SPI_CTRLA_ENABLE;
    waitForSpiSync(sercom_hw, SERCOM_SPI_SYNCBUSY_ENABLE);
}

U32 SpiHal::getDataRegisterAddress(SercomKind sercom) {
    auto* sercom_hw = SercomUtil::getHardware(sercom);
    FW_ASSERT(sercom_hw != nullptr, sercom.e);
    return reinterpret_cast<U32>(&sercom_hw->SPI.DATA);
}

}  // namespace SpiHardware
}  // namespace Samd21

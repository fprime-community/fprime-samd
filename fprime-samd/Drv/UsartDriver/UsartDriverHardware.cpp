// ======================================================================
// \title  UsartDriverHardware.cpp
// \author tumbar
// \brief  MCU-specific hardware implementation for USART peripheral
//
// This file is only compiled for SAMD21 target builds.
// For Linux/test builds, UsartDriverHardwareStub.cpp is used instead.
// ======================================================================

#include "fprime-samd/Drv/UsartDriver/UsartDriverHardware.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/Types/Sercom.hpp"

namespace Samd21 {
namespace UsartHardware {

// ----------------------------------------------------------------------
// File-local helpers
// ----------------------------------------------------------------------

// Bound hardware synchronization waits to ~1s (F_CPU cycles), matching the
// pattern used in RtcDriver/DmaDriver. A stuck sync flag asserts rather than
// hanging the CPU forever.
static void waitForGclkSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForUsartSync(Sercom* sercom_hw, U32 mask) {
    volatile U32 limit = F_CPU;
    while (limit > 0 && (sercom_hw->USART.SYNCBUSY.reg & mask)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForUsartFlag(Sercom* sercom_hw, U32 mask) {
    volatile U32 limit = F_CPU;
    while (limit > 0 && !(sercom_hw->USART.INTFLAG.reg & mask)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

//! Calculate BAUD register value based on baud rate and mode
static U16 calculateBaud(UsartDriver::BaudRate baud_rate, UsartDriver::CommunicationMode mode) {
    const U64 f_ref = F_CPU;
    const U32 f_baud = static_cast<U32>(baud_rate);
    U16 baud_value;

    switch (mode) {
        case UsartDriver::CommunicationMode::ASYNC: {
            // Asynchronous Arithmetic mode (§27.6.2.3, Table 26-2)
            // BAUD = 65536 * (1 - S * (f_BAUD / f_ref))
            // where S = 16 (oversampling)
            const U32 S = 16;
            U64 top = 65536ULL * S * static_cast<U64>(f_baud);
            baud_value = 65536 - static_cast<U64>(top / f_ref);
            break;
        }
        case UsartDriver::CommunicationMode::SYNC:
            // Synchronous mode (§27.6.2.3, Table 26-2)
            // BAUD = (f_ref / (2 * f_BAUD))
            baud_value = f_ref / (2 * f_baud);

            // Validate range
            FW_ASSERT(baud_value <= 255, baud_value);
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(mode));
    }

    return baud_value;
}

// ----------------------------------------------------------------------
// UsartHal implementation
// ----------------------------------------------------------------------

void UsartHal::configure(SercomKind sercom,
                         UsartDriver::RxPinOut rx,
                         UsartDriver::TxPinOut tx,
                         UsartDriver::ClockMode clock,
                         UsartDriver::CommunicationMode mode,
                         UsartDriver::BaudRate baud_rate,
                         UsartDriver::DataOrder data_order,
                         UsartDriver::DataBits data_bits,
                         UsartDriver::StopBits stop_bits,
                         UsartDriver::Parity parity) {
    // Get SERCOM hardware register base
    Sercom* sercom_hw = SercomUtil::getHardware(sercom);
    FW_ASSERT(sercom_hw != nullptr, sercom);

    // Enable SERCOM peripheral clock (APBC bus)
    // Per §8.6 PM – Power Manager and datasheet Table 14-2
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

    // Assign Generic Clock Generator 0 (48MHz) to all sercom
    // Per §14.8.3 GCLK_CLKCTRL – Generic Clock Control
    U8 gclk_id = static_cast<U8>(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val) + sercom.e;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id) |
                        GCLK_CLKCTRL_GEN_GCLK0 |  // Use Generic Clock Generator 0 (48MHz main clock)
                        GCLK_CLKCTRL_CLKEN;
    waitForGclkSync();

    // Build CTRLA and CTRLB registers per §27.6.2.1 initialization sequence
    // Steps 1-4, 6, 7a configure CTRLA; steps 5, 7b, 8, 10 configure CTRLB
    SERCOM_USART_CTRLA_Type ctrla = {.reg = 0};
    SERCOM_USART_CTRLB_Type ctrlb = {.reg = 0};

    // Step 1: Select operating mode
    switch (clock) {
        case UsartDriver::ClockMode::EXTERNAL:
            ctrla.bit.MODE = SERCOM_USART_CTRLA_MODE_USART_EXT_CLK_Val;
            break;
        case UsartDriver::ClockMode::INTERNAL:
            ctrla.bit.MODE = SERCOM_USART_CTRLA_MODE_USART_INT_CLK_Val;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(clock));
    }

    // Step 2: Select communication mode
    switch (mode) {
        case UsartDriver::CommunicationMode::ASYNC:
            ctrla.bit.CMODE = 0;  // Asynchronous
            break;
        case UsartDriver::CommunicationMode::SYNC:
            ctrla.bit.CMODE = 1;  // Synchronous
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(mode));
    }

    // Step 3: Select RX pin
    switch (rx) {
        case UsartDriver::RxPinOut::PAD0:
            ctrla.bit.RXPO = 0;  // RxD on PAD[0]
            break;
        case UsartDriver::RxPinOut::PAD1:
            ctrla.bit.RXPO = 1;  // RxD on PAD[1]
            break;
        case UsartDriver::RxPinOut::PAD2:
            ctrla.bit.RXPO = 2;  // RxD on PAD[2]
            break;
        case UsartDriver::RxPinOut::PAD3:
            ctrla.bit.RXPO = 3;  // RxD on PAD[3]
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(rx));
    }

    // Step 4: Select TX pin
    switch (tx) {
        case UsartDriver::TxPinOut::PAD0:
            ctrla.bit.TXPO = 0;  // TxD on PAD[0], XCK on PAD[1]
            break;
        case UsartDriver::TxPinOut::PAD2_XCK_PAD3:
            ctrla.bit.TXPO = 1;  // TxD on PAD[2], XCK on PAD[3]
            break;
        case UsartDriver::TxPinOut::PAD0_RTS_PAD2_CTS_PAD3:
            ctrla.bit.TXPO = 2;  // TxD on PAD[0], RTS on PAD[2], CTS on PAD[3]
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(tx));
    }

    // Step 5: Configure character size
    switch (data_bits) {
        case UsartDriver::DataBits::BITS_5:
            ctrlb.bit.CHSIZE = 5;  // 5 data bits
            break;
        case UsartDriver::DataBits::BITS_6:
            ctrlb.bit.CHSIZE = 6;  // 6 data bits
            break;
        case UsartDriver::DataBits::BITS_7:
            ctrlb.bit.CHSIZE = 7;  // 7 data bits
            break;
        case UsartDriver::DataBits::BITS_8:
            ctrlb.bit.CHSIZE = 0;  // 8 data bits (default)
            break;
        case UsartDriver::DataBits::BITS_9:
            ctrlb.bit.CHSIZE = 1;  // 9 data bits
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_bits));
    }

    // Step 6: Set data order
    switch (data_order) {
        case UsartDriver::DataOrder::MSB_FIRST:
            ctrla.bit.DORD = 0;  // MSB transmitted first
            break;
        case UsartDriver::DataOrder::LSB_FIRST:
            ctrla.bit.DORD = 1;  // LSB transmitted first
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(data_order));
    }

    // Step 7a: Enable parity mode
    switch (parity) {
        case UsartDriver::Parity::NONE:
            ctrla.bit.FORM = 0;  // USART frame without parity
            break;
        case UsartDriver::Parity::EVEN:
        case UsartDriver::Parity::ODD:
            ctrla.bit.FORM = 1;  // USART frame with parity
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(parity));
    }

    // Step 7b: Configure parity mode
    switch (parity) {
        case UsartDriver::Parity::NONE:
        case UsartDriver::Parity::EVEN:
            ctrlb.bit.PMODE = 0;  // Even parity
            break;
        case UsartDriver::Parity::ODD:
            ctrlb.bit.PMODE = 1;  // Odd parity
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(parity));
    }

    // Step 8: Configure stop bits (CTRLB.SBMODE: 0=1 stop bit, 1=2 stop bits)
    switch (stop_bits) {
        case UsartDriver::StopBits::ONE:
            ctrlb.bit.SBMODE = 0;  // 1 stop bit
            break;
        case UsartDriver::StopBits::TWO:
            ctrlb.bit.SBMODE = 1;  // 2 stop bits
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(stop_bits));
    }

    ctrla.bit.SAMPR = 0;
    ctrlb.bit.RXEN = 1;
    ctrlb.bit.TXEN = 1;

    // § 27.6.2.1 Initialization: These registers are enable-protected and can only
    // be written when CTRLA.ENABLE=0

    // Disable USART if currently enabled
    sercom_hw->USART.CTRLA.bit.ENABLE = 0;
    // Wait for synchronization
    waitForUsartSync(sercom_hw, SERCOM_USART_SYNCBUSY_ENABLE);

    sercom_hw->USART.CTRLA.reg |= SERCOM_USART_CTRLA_SWRST;
    waitForUsartSync(sercom_hw, SERCOM_USART_SYNCBUSY_SWRST);

    // Write CTRLA register (steps 1-4, 6, 7a)
    sercom_hw->USART.CTRLA.reg = ctrla.reg;

    waitForUsartSync(sercom_hw, SERCOM_USART_SYNCBUSY_ENABLE);

    // Write CTRLB register (steps 5, 7b, 8)
    sercom_hw->USART.CTRLB.reg = ctrlb.reg;
    waitForUsartSync(sercom_hw, SERCOM_USART_SYNCBUSY_CTRLB);

    // Step 9: Configure baud rate (only for internal clock)
    if (clock == UsartDriver::ClockMode::INTERNAL) {
        U16 baud_value = calculateBaud(baud_rate, mode);
        sercom_hw->USART.BAUD.reg = baud_value;
    }

    // Enable USART
    sercom_hw->USART.CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
    waitForUsartSync(sercom_hw, SERCOM_USART_SYNCBUSY_ENABLE);

    // Wait for the Tx and Rx lines to be enabled
    waitForUsartSync(sercom_hw, SERCOM_USART_SYNCBUSY_CTRLB);
}

void UsartHal::sendSync(SercomKind sercom, const U8* data, U32 size) {
    Sercom* sercom_hw = SercomUtil::getHardware(sercom);
    for (U32 i = 0; i < size; i++) {
        waitForUsartFlag(sercom_hw, SERCOM_USART_INTFLAG_DRE);
        sercom_hw->USART.DATA.reg = data[i];
    }

    // Wait for the final byte to transmit
    waitForUsartFlag(sercom_hw, SERCOM_USART_INTFLAG_DRE);
}

U32 UsartHal::getDataRegisterAddress(SercomKind sercom) {
    // Note: Use DATA.reg to get the actual register address, not the structure address
    return reinterpret_cast<U32>(&SercomUtil::getHardware(sercom)->USART.DATA.reg);
}

bool UsartHal::checkAndClearRxOverflow(SercomKind sercom) {
    Sercom* sercom_hw = SercomUtil::getHardware(sercom);

    // STATUS.BUFOVF is a sticky, write-1-to-clear flag (§27.8.5). Read it, then
    // clear it by writing the bit back so the next check reports fresh overflows.
    const bool overflowed = (sercom_hw->USART.STATUS.reg & SERCOM_USART_STATUS_BUFOVF) != 0;
    if (overflowed) {
        sercom_hw->USART.STATUS.reg = SERCOM_USART_STATUS_BUFOVF;
    }
    return overflowed;
}

}  // namespace UsartHardware
}  // namespace Samd21

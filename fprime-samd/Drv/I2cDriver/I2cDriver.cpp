// ======================================================================
// \title  I2cDriver.cpp
// \author tumbar
// \brief  cpp file for I2cDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "fprime-samd/Drv/Types/SercomIsr.hpp"
#include "samd.h"

namespace Samd21 {

//! Map a SercomKind to its hardware register base
static Sercom* getSercomHw(SercomKind sercom) {
    switch (sercom.e) {
        case SercomKind::SERCOM_0:
            return SERCOM0;
        case SercomKind::SERCOM_1:
            return SERCOM1;
        case SercomKind::SERCOM_2:
            return SERCOM2;
        case SercomKind::SERCOM_3:
            return SERCOM3;
            // Some SAMD21 devices don't have these SERCOM ports
#ifdef SERCOM4
        case SercomKind::SERCOM_4:
            return SERCOM4;
#endif
#ifdef SERCOM5
        case SercomKind::SERCOM_5:
            return SERCOM5;
#endif
        default:
            FW_ASSERT(false, sercom.e);
            return nullptr;
    }
}

// Bound hardware synchronization waits to ~1s (F_CPU cycles), matching the
// pattern used in RtcDriver/DmaDriver/UsartDriver. A stuck sync flag asserts
// rather than hanging the CPU forever.
static void waitForGclkSync() {
    volatile U32 limit = F_CPU;
    while (limit > 0 && GCLK->STATUS.bit.SYNCBUSY) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

static void waitForI2cSync(Sercom* sercom_hw, U32 mask) {
    volatile U32 limit = F_CPU;
    while (limit > 0 && (sercom_hw->I2CM.SYNCBUSY.reg & mask)) {
        limit--;
    }

    // Check if we timed out
    FW_ASSERT(limit != 0);
}

//! Map the target SCL Frequency to the CTRLA.SPEED transfer-mode field.
//!
//! CTRLA.SPEED only selects the electrical/protocol timing mode; the actual
//! SCL rate comes from the BAUD register. See §29.10.1 (CTRLA.SPEED).
static U8 speedFieldFor(I2cDriver::Frequency frequency) {
    switch (frequency) {
        case I2cDriver::Frequency::STANDARD_100KHZ:
        case I2cDriver::Frequency::FAST_400KHZ:
            return 0x0;  // Sm/Fm up to 400 kHz
        case I2cDriver::Frequency::FAST_PLUS_1MHZ:
            return 0x1;  // Fm+ up to 1 MHz
        case I2cDriver::Frequency::HIGH_SPEED_3400KHZ:
            return 0x2;  // Hs up to 3.4 MHz
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(frequency));
            return 0x0;
    }
}

//! Compute the BAUD register value for Sm/Fm/Fm+ modes.
//!
//! From §29.6.2.4.1, with BAUD.BAUDLOW = 0 the BAUD field times both the SCL
//! high and low periods:
//!
//!     fSCL = fGCLK / (10 + 2*BAUD + fGCLK*Trise)
//!
//! Solving for BAUD and assuming Trise = 0 (conservative -- ignoring rise time
//! only makes the divisor smaller, i.e. the computed BAUD larger, so the real
//! SCL comes out slightly SLOWER than target and never overclocks the bus):
//!
//!     BAUD = (fGCLK / (2*fSCL)) - 5
//!
//! The result is clamped to the 8-bit BAUD field.
static U8 calculateBaud(I2cDriver::Frequency frequency) {
    const U32 f_gclk = F_CPU;
    const U32 f_scl = static_cast<U32>(frequency);

    // BAUD = fGCLK / (2*fSCL) - 5
    const U32 half = f_gclk / (2 * f_scl);
    // Guard against the subtraction underflowing for very slow GCLK / fast SCL
    FW_ASSERT(half > 5, half);
    const U32 baud = half - 5;

    // BAUD is an 8-bit field
    FW_ASSERT(baud <= 255, baud);
    return static_cast<U8>(baud);
}

//! Compute the HSBAUD register value for High-speed mode.
//!
//! From §29.6.2.4.1, with HSBAUDLOW = 0:
//!
//!     fSCL = fGCLK / (2 + 2*HSBAUD)   =>   HSBAUD = fGCLK/(2*fSCL) - 1
static U8 calculateHsBaud(I2cDriver::Frequency frequency) {
    const U32 f_gclk = F_CPU;
    const U32 f_scl = static_cast<U32>(frequency);

    const U32 half = f_gclk / (2 * f_scl);
    FW_ASSERT(half > 1, half);
    const U32 hsbaud = half - 1;

    FW_ASSERT(hsbaud <= 255, hsbaud);
    return static_cast<U8>(hsbaud);
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

I2cDriver ::I2cDriver(const char* const compName)
    : I2cDriverComponentBase(compName), m_sercom(SercomKind::SERCOM_0), m_configured(false) {}

I2cDriver ::~I2cDriver() {}

void I2cDriver::configure(SercomKind sercom,
                          SclLowTimeout scl_low_timeout,
                          InactiveTimeout inactive_timeout,
                          ClockStretchMode clock_stretch_mode,
                          Frequency frequency,
                          ClientSclLowTimeout client_scl_low_timeout,
                          HostSclLowTimeout host_scl_low_timeout,
                          SdaHold sda_hold,
                          PinUsage pin_usage,
                          RunInStandby run_in_standby) {
    FW_ASSERT(!this->m_configured, sercom.e);

    // Store configuration
    m_sercom = sercom;

    // Register with the ISR callback table
    Samd21::SercomIsr::registerHandler(sercom, i2cDriverIsrHandler, this);

    // Get SERCOM hardware register base
    Sercom* sercom_hw = getSercomHw(sercom);
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

    // Assign Generic Clock Generator 0 (48MHz) to the SERCOM core clock.
    // GCLK_SERCOMx_CORE clocks the I2C host baud-rate generator (§29.5.3).
    // Per §14.8.3 GCLK_CLKCTRL – Generic Clock Control
    U8 gclk_id = static_cast<U8>(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val) + sercom.e;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id) |
                        GCLK_CLKCTRL_GEN_GCLK0 |  // Use Generic Clock Generator 0 (48MHz main clock)
                        GCLK_CLKCTRL_CLKEN;
    waitForGclkSync();

    // Build CTRLA and CTRLB registers per §29.6.2.1 initialization sequence.
    // These registers are enable-protected: they can only be written while
    // CTRLA.ENABLE=0.
    SERCOM_I2CM_CTRLA_Type ctrla = {.reg = 0};
    SERCOM_I2CM_CTRLB_Type ctrlb = {.reg = 0};

    // Step 1: Select I2C Host (master) operating mode (CTRLA.MODE = 0x5)
    ctrla.bit.MODE = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER_Val;

    // Step 2: SDA hold time (CTRLA.SDAHOLD)
    switch (sda_hold) {
        case I2cDriver::SdaHold::DISABLED:
            ctrla.bit.SDAHOLD = 0x0;
            break;
        case I2cDriver::SdaHold::HOLD_75_NS:
            ctrla.bit.SDAHOLD = 0x1;
            break;
        case I2cDriver::SdaHold::HOLD_450_NS:
            ctrla.bit.SDAHOLD = 0x2;
            break;
        case I2cDriver::SdaHold::HOLD_600_NS:
            ctrla.bit.SDAHOLD = 0x3;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(sda_hold));
    }

    // Step 4: SCL low time-out (CTRLA.LOWTOUTEN)
    switch (scl_low_timeout) {
        case I2cDriver::SclLowTimeout::DISABLED:
            ctrla.bit.LOWTOUTEN = 0;
            break;
        case I2cDriver::SclLowTimeout::ENABLED:
            ctrla.bit.LOWTOUTEN = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(scl_low_timeout));
    }

    // Step 5a (Host mode): Inactive bus time-out (CTRLA.INACTOUT)
    switch (inactive_timeout) {
        case I2cDriver::InactiveTimeout::DISABLED:
            ctrla.bit.INACTOUT = 0x0;
            break;
        case I2cDriver::InactiveTimeout::TIMEOUT_55_US:
            ctrla.bit.INACTOUT = 0x1;
            break;
        case I2cDriver::InactiveTimeout::TIMEOUT_105_US:
            ctrla.bit.INACTOUT = 0x2;
            break;
        case I2cDriver::InactiveTimeout::TIMEOUT_205_US:
            ctrla.bit.INACTOUT = 0x3;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(inactive_timeout));
    }

    // Transfer speed / clock-stretch mode (CTRLA.SPEED, CTRLA.SCLSM).
    // High-speed mode requires CTRLA.SCLSM=1 (§29.6.2.4, Note).
    const U8 speed_field = speedFieldFor(frequency);
    ctrla.bit.SPEED = speed_field;

    switch (clock_stretch_mode) {
        case I2cDriver::ClockStretchMode::ALWAYS:
            ctrla.bit.SCLSM = 0;
            break;
        case I2cDriver::ClockStretchMode::AFTER_ACK:
            ctrla.bit.SCLSM = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(clock_stretch_mode));
    }

    if (frequency == I2cDriver::Frequency::HIGH_SPEED_3400KHZ) {
        // Hs mode mandates SCL stretch only after ACK regardless of request.
        ctrla.bit.SCLSM = 1;
    }

    // Client/Host SCL low extend time-outs (CTRLA.SEXTTOEN / CTRLA.MEXTTOEN)
    switch (client_scl_low_timeout) {
        case I2cDriver::ClientSclLowTimeout::DISABLED:
            ctrla.bit.SEXTTOEN = 0;
            break;
        case I2cDriver::ClientSclLowTimeout::ENABLED:
            ctrla.bit.SEXTTOEN = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(client_scl_low_timeout));
    }

    switch (host_scl_low_timeout) {
        case I2cDriver::HostSclLowTimeout::DISABLED:
            ctrla.bit.MEXTTOEN = 0;
            break;
        case I2cDriver::HostSclLowTimeout::ENABLED:
            ctrla.bit.MEXTTOEN = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(host_scl_low_timeout));
    }

    // Pin usage: two- or four-wire operation (CTRLA.PINOUT)
    switch (pin_usage) {
        case I2cDriver::PinUsage::TWO_WIRE:
            ctrla.bit.PINOUT = 0;
            break;
        case I2cDriver::PinUsage::FOUR_WIRE:
            ctrla.bit.PINOUT = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(pin_usage));
    }

    // Run in standby (CTRLA.RUNSTDBY)
    switch (run_in_standby) {
        case I2cDriver::RunInStandby::DISABLED:
            ctrla.bit.RUNSTDBY = 0;
            break;
        case I2cDriver::RunInStandby::ENABLED:
            ctrla.bit.RUNSTDBY = 1;
            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(run_in_standby));
    }

    // CTRLB: enable smart mode so the host auto-ACKs during reads (§29.6.2.4).
    ctrlb.bit.SMEN = 1;

    // §29.6.2.1 Initialization: enable-protected registers require ENABLE=0.

    // Reset the peripheral to a known state before configuring.
    sercom_hw->I2CM.CTRLA.bit.ENABLE = 0;
    waitForI2cSync(sercom_hw, SERCOM_I2CM_SYNCBUSY_ENABLE);

    sercom_hw->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_SWRST;
    waitForI2cSync(sercom_hw, SERCOM_I2CM_SYNCBUSY_SWRST);

    // Write CTRLA (mode, timeouts, speed, pinout, sda hold, standby)
    sercom_hw->I2CM.CTRLA.reg = ctrla.reg;

    // Write CTRLB (smart mode). ACKACT/CMD are not enable-protected.
    sercom_hw->I2CM.CTRLB.reg = ctrlb.reg;
    waitForI2cSync(sercom_hw, SERCOM_I2CM_SYNCBUSY_SYSOP);

    // Step 5b (Host mode): program the baud rate to generate the target SCL.
    if (frequency == I2cDriver::Frequency::HIGH_SPEED_3400KHZ) {
        // Hs uses the HSBAUD field; leave BAUD/BAUDLOW for the Fs (arbitration)
        // phase and HSBAUDLOW=0 so HSBAUD times both high and low.
        sercom_hw->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_HSBAUD(calculateHsBaud(frequency));
    } else {
        // Sm/Fm/Fm+: BAUDLOW=0 so BAUD times both high and low periods.
        sercom_hw->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(calculateBaud(frequency));
    }

    // Enable the peripheral.
    sercom_hw->I2CM.CTRLA.reg |= SERCOM_I2CM_CTRLA_ENABLE;
    waitForI2cSync(sercom_hw, SERCOM_I2CM_SYNCBUSY_ENABLE);

    // After enable the bus state is UNKNOWN (0b00). Force it to IDLE (0b01) so
    // the host is ready to start a transaction (§29.6.2.3).
    sercom_hw->I2CM.STATUS.bit.BUSSTATE = 0x1;
    waitForI2cSync(sercom_hw, SERCOM_I2CM_SYNCBUSY_SYSOP);

    this->m_configured = true;
}

void i2cDriverIsrHandler(SercomKind sercom, void* i2cDriverRaw) {
    auto i2cDriver = reinterpret_cast<I2cDriver*>(i2cDriverRaw);

    FW_ASSERT(i2cDriver != nullptr);
    FW_ASSERT(sercom == i2cDriver->m_sercom, sercom, i2cDriver->m_sercom);
    i2cDriver->isrHandler();
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void I2cDriver ::read_handler(FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    // TODO
}

void I2cDriver ::write_handler(FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    // TODO
}

void I2cDriver ::writeRead_handler(FwIndexType portNum, U32 addr, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    // TODO
}

}  // namespace Samd21

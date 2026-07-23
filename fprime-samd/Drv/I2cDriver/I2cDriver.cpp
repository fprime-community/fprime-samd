// ======================================================================
// \title  I2cDriver.cpp
// \author tumbar
// \brief  cpp file for I2cDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include <samd21/include/samd21g17a.h>
#include "Drv/Types/SercomKindEnumAc.hpp"
#include "Drv/Types/TriggerSourceEnumAc.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/Types/SercomIsr.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
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

static Samd21::Dma::TriggerSource getReadTriggerSource(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return Samd21::Dma::TriggerSource::SERCOM0_RX;
        case SercomKind::SERCOM_1:
            return Samd21::Dma::TriggerSource::SERCOM1_RX;
        case SercomKind::SERCOM_2:
            return Samd21::Dma::TriggerSource::SERCOM2_RX;
        case SercomKind::SERCOM_3:
            return Samd21::Dma::TriggerSource::SERCOM3_RX;
        case SercomKind::SERCOM_4:
            return Samd21::Dma::TriggerSource::SERCOM4_RX;
        case SercomKind::SERCOM_5:
            return Samd21::Dma::TriggerSource::SERCOM5_RX;
        default:
            FW_ASSERT(false, sercom);
    }
}

static Samd21::Dma::TriggerSource getWriteTriggerSource(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return Samd21::Dma::TriggerSource::SERCOM0_TX;
        case SercomKind::SERCOM_1:
            return Samd21::Dma::TriggerSource::SERCOM1_TX;
        case SercomKind::SERCOM_2:
            return Samd21::Dma::TriggerSource::SERCOM2_TX;
        case SercomKind::SERCOM_3:
            return Samd21::Dma::TriggerSource::SERCOM3_TX;
        case SercomKind::SERCOM_4:
            return Samd21::Dma::TriggerSource::SERCOM4_TX;
        case SercomKind::SERCOM_5:
            return Samd21::Dma::TriggerSource::SERCOM5_TX;
        default:
            FW_ASSERT(false, sercom);
    }
}

static ::IRQn_Type getSercomIrq(SercomKind sercom) {
    switch (sercom) {
        case SercomKind::SERCOM_0:
            return IRQn_Type::SERCOM0_IRQn;
        case SercomKind::SERCOM_1:
            return IRQn_Type::SERCOM1_IRQn;
        case SercomKind::SERCOM_2:
            return IRQn_Type::SERCOM2_IRQn;
        case SercomKind::SERCOM_3:
            return IRQn_Type::SERCOM3_IRQn;
#ifdef SERCOM4
        case SercomKind::SERCOM_4:
            return IRQn_Type::SERCOM4_IRQn;
#endif
#ifdef SERCOM5
        case SercomKind::SERCOM_5:
            return IRQn_Type::SERCOM5_IRQn;
#endif
        default:
            FW_ASSERT(false, sercom.e);
            return IRQn_Type::SERCOM0_IRQn;
    }
}

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

I2cDriver ::I2cDriver(const char* const compName)
    : I2cDriverComponentBase(compName),
      m_sercom(SercomKind::SERCOM_0),
      m_configured(false),
      m_state(I2cDriver::State::IDLE),
      m_portNum(),
      m_read(),
      m_pending_read_address(0),
      m_write() {}

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
    FW_ASSERT(this->m_state == State::IDLE, static_cast<FwAssertArgType>(this->m_state));

    // Store configuration
    this->m_sercom = sercom;

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

    // Enable only the ERROR interrupt for this sercom device
    sercom_hw->I2CM.INTENSET.reg = SERCOM_I2CM_INTENSET_ERROR;

    // Enable I2C interrupt at lowest priority
    static constexpr U32 LOWEST_PRIORITY = (1U << __NVIC_PRIO_BITS) - 1;
    auto irqn = getSercomIrq(sercom);
    NVIC_EnableIRQ(irqn);
    NVIC_SetPriority(irqn, LOWEST_PRIORITY);

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

void I2cDriver ::isrHandler() {
    auto sercom_hw = getSercomHw(this->m_sercom);

    // Check the source of the SERCOM ISR
    // _Only_ the error interrupt should have triggered us...
    FW_ASSERT(sercom_hw->I2CM.INTFLAG.reg == SERCOM_I2CM_INTENSET_ERROR,
              static_cast<FwAssertArgType>(sercom_hw->I2CM.INTFLAG.reg));

    // Look up the error
    if (sercom_hw->I2CM.STATUS.bit.BUSERR) {
        this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cHostError::BusError);
        sercom_hw->I2CM.STATUS.bit.BUSERR = 1;  // ack the error
    }

    if (sercom_hw->I2CM.STATUS.bit.ARBLOST) {
        this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cHostError::ArbitrationLost);
        sercom_hw->I2CM.STATUS.bit.ARBLOST = 1;  // ack the error
    }

    if (sercom_hw->I2CM.STATUS.bit.LOWTOUT) {
        this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cHostError::SclLowTimeout);
        sercom_hw->I2CM.STATUS.bit.LOWTOUT = 1;  // ack the error
    }

    if (sercom_hw->I2CM.STATUS.bit.MEXTTOUT) {
        this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cHostError::MasterSclExtendTimeout);
        sercom_hw->I2CM.STATUS.bit.MEXTTOUT = 1;  // ack the error
    }

    if (sercom_hw->I2CM.STATUS.bit.SEXTTOUT) {
        this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cHostError::SlaveSclExtendTimeout);
        sercom_hw->I2CM.STATUS.bit.SEXTTOUT = 1;  // ack the error
    }

    if (sercom_hw->I2CM.STATUS.bit.LENERR) {
        this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cHostError::LengthError);
        sercom_hw->I2CM.STATUS.bit.LENERR = 1;  // ack the error
    }

    // Acknowledge the error flag
    sercom_hw->I2CM.INTFLAG.bit.ERROR = 1;

    switch (this->m_state) {
        case State::IDLE:
            // We got an interrupt when we were not expecting to
            FW_ASSERT(false, this->m_sercom);
            break;
        case State::READ: {
            auto buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;
            this->dmaTransactionAbortOut_out(0);
            this->readComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_READ_ERR);
            break;
        }
        case State::WRITE: {
            auto buf = this->m_write.getBuffer();
            this->m_state = State::IDLE;
            this->dmaTransactionAbortOut_out(0);
            this->writeComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_WRITE_ERR);
            break;
        }
        case State::WRITE_READ_WRITING: {
            auto w_buf = this->m_write.getBuffer();
            auto r_buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;
            this->dmaTransactionAbortOut_out(0);
            this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_WRITE_ERR);
            break;
        }
        case State::WRITE_READ_READING: {
            auto w_buf = this->m_write.getBuffer();
            auto r_buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;
            this->dmaTransactionAbortOut_out(0);
            this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_READ_ERR);
            break;
        }
        default:
            FW_ASSERT(false, this->m_sercom, static_cast<FwAssertArgType>(this->m_state));
    }
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void I2cDriver ::dmaReplyIn_handler(FwIndexType portNum, const Samd21::Dma::Reply& reply) {
    switch (this->m_state) {
        case State::IDLE:
            // We got a DMA response when we were not expecting it
            FW_ASSERT(false, this->m_sercom);
            break;
        case State::READ: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            auto buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;
            this->readComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_OK);
            break;
        }
        case State::WRITE: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            auto buf = this->m_write.getBuffer();
            this->m_state = State::IDLE;
            this->writeComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_OK);
            break;
        }
        case State::WRITE_READ_WRITING: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);

            // The write of the write/read chain finished
            // Proceed to read.
            this->m_state = State::WRITE_READ_READING;

            auto buf = this->m_read.getBuffer();
            this->readImpl(this->m_pending_read_address, buf);
            break;
        }
        case State::WRITE_READ_READING: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            auto w_buf = this->m_write.getBuffer();
            auto r_buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;

            this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_OK);
            break;
        }
        default:
            FW_ASSERT(false, this->m_sercom, static_cast<FwAssertArgType>(this->m_state));
    }
}

void I2cDriver ::read_handler(FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    if (this->m_state != State::IDLE) {
        // TODO(tumbar) Technically this is a programming error... maybe we should assert?
        this->readComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }

    this->m_state = State::READ;
    this->m_read = ThinBuffer(buffer);
    this->m_portNum = portNum;

    this->readImpl(addr, buffer);
}

void I2cDriver::readImpl(U32 addr, Fw::Buffer& buffer) {
    Sercom* sercom_hw = getSercomHw(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    // I2C DMA is limited to 255 bytes
    FW_ASSERT(buffer.getSize() <= 255, buffer.getSize());

    // TODO(tumbar) We currently only support 7-bit address mode
    FW_ASSERT((addr & ~0x7F) == 0, addr);

    // Send ACK to data coming back from the peripheral
    sercom_hw->I2CM.CTRLB.bit.ACKACT = 0;

    // Queue up a DMA operation to read the data from the device
    this->dmaTransactionOut_out(0, getReadTriggerSource(this->m_sercom), Samd21::Dma::TransactionType::BEAT,
                                Samd21::Dma::Priority::PRIORITY_1, reinterpret_cast<U32>(&sercom_hw->I2CM.DATA),
                                reinterpret_cast<U32>(buffer.getData()), buffer.getSize(), Samd21::Dma::BeatSize::BYTE,
                                /* incrementSource */ false, /* incrementDestination */ true,
                                Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::DESTINATION);

    SERCOM_I2CM_ADDR_Type addrReg = {};
    addrReg.bit.HS = 0;     // We do not currently support high speed mode
    addrReg.bit.LENEN = 1;  // Enable length mode to generate DMA requests
    addrReg.bit.LEN = static_cast<U8>(buffer.getSize());
    addrReg.bit.ADDR = (addr << 1) | 0x1;  // send a read request

    // Kick off the I2C job by writing the address
    sercom_hw->I2CM.ADDR.reg = addrReg.reg;
}

void I2cDriver::writeImpl(U32 addr, Fw::Buffer& buffer) {
    Sercom* sercom_hw = getSercomHw(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    // I2C DMA is limited to 255 bytes
    FW_ASSERT(buffer.getSize() <= 255, buffer.getSize());

    // TODO(tumbar) We currently only support 7-bit address mode
    FW_ASSERT((addr & ~0x7F) == 0, addr);

    // Send NACK if anyone tries to send data to us
    sercom_hw->I2CM.CTRLB.bit.ACKACT = 1;

    // Queue up a DMA operation to read the data from the device
    this->dmaTransactionOut_out(0, getWriteTriggerSource(this->m_sercom), Samd21::Dma::TransactionType::BEAT,
                                Samd21::Dma::Priority::PRIORITY_1, reinterpret_cast<U32>(buffer.getData()),
                                reinterpret_cast<U32>(&sercom_hw->I2CM.DATA), buffer.getSize(),
                                Samd21::Dma::BeatSize::BYTE,
                                /* incrementSource */ true, /* incrementDestination */ false,
                                Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::SOURCE);

    SERCOM_I2CM_ADDR_Type addrReg = {};
    addrReg.bit.HS = 0;     // We do not currently support high speed mode
    addrReg.bit.LENEN = 1;  // Enable length mode to generate DMA requests
    addrReg.bit.LEN = static_cast<U8>(buffer.getSize());
    addrReg.bit.ADDR = (addr << 1) | 0x0;  // send a write request

    // Kick off the I2C job by writing the address
    sercom_hw->I2CM.ADDR.reg = addrReg.reg;
}

void I2cDriver ::write_handler(FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    if (this->m_state != State::IDLE) {
        // TODO(tumbar) Technically this is a programming error... maybe we should assert?
        this->readComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }

    this->m_state = State::WRITE;
    this->m_write = ThinBuffer(buffer);
    this->m_portNum = portNum;

    this->writeImpl(addr, buffer);
}

void I2cDriver ::writeRead_handler(FwIndexType portNum, U32 addr, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    if (this->m_state != State::IDLE) {
        // TODO(tumbar) Technically this is a programming error... maybe we should assert?
        this->writeReadComplete_out(portNum, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OTHER_ERR);
        return;
    }

    this->m_state = State::WRITE_READ_WRITING;
    this->m_read = ThinBuffer(readBuffer);
    this->m_write = ThinBuffer(writeBuffer);
    this->m_portNum = portNum;

    this->writeImpl(addr, writeBuffer);
}

}  // namespace Samd21

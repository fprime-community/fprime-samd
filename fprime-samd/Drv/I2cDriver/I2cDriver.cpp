// ======================================================================
// \title  I2cDriver.cpp
// \author tumbar
// \brief  cpp file for I2cDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "fprime-samd/Drv/Types/StatusEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"

namespace Samd21 {

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
      m_write(),
      m_tlmErrors(0) {}

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
    SercomUtil::registerIsrHandler(sercom, I2cDriver::s_i2cDriverIsrHandler, this);

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

    // Assign a 32kHz generator to the shared SERCOMx_SLOW clock. This clock drives
    // the SMBus SCL-low / bus-inactive / SCL-extend time-out counters (§28.6.3.1:
    // "These time-outs are driven by the GCLK_SERCOM_SLOW clock ... must be
    // configured to use a 32KHz oscillator"). Without it, every time-out we enable
    // in CTRLA is dead -- including the SCL-low time-out that is supposed to auto-
    // recover a bus a client is holding low, so a stuck bus would hang forever.
    //
    // SERCOMX_SLOW is a SINGLE clock shared by all SERCOM instances, so this is set
    // once for the whole peripheral (writing it again per-instance is harmless).
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_SERCOMX_SLOW | GCLK_CLKCTRL_GEN_GCLK1 | GCLK_CLKCTRL_CLKEN;
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

void I2cDriver::s_i2cDriverIsrHandler(Fw::PassiveComponentBase* i2cDriverRaw, SercomKind sercom) {
    auto i2cDriver = reinterpret_cast<I2cDriver*>(i2cDriverRaw);

    FW_ASSERT(i2cDriver != nullptr);
    FW_ASSERT(sercom == i2cDriver->m_sercom, sercom, i2cDriver->m_sercom);
    i2cDriver->isrHandler();
}

void I2cDriver ::isrHandler() {
    auto sercom_hw = SercomUtil::getHardware(this->m_sercom);

    // There are two interrupt sources:
    // 1. Errors on the I2C bus
    // 2. Post-Tx ACK from the slave during a writeRead
    //
    // We need to handle both cases in the same handler

    if (sercom_hw->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_ERROR) {
        // The interrupt source was an error
        // The other interrupt source _could_ still be set but we should ack it and ignore it

        if (sercom_hw->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB) {
            // Acknowledge the master on bus
            sercom_hw->I2CM.INTFLAG.bit.MB = 1;
        }

        // Look up the error
        if (sercom_hw->I2CM.STATUS.bit.BUSERR) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::BUS_ERROR);
            sercom_hw->I2CM.STATUS.bit.BUSERR = 1;  // ack the error
        }

        if (sercom_hw->I2CM.STATUS.bit.ARBLOST) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::ARBITRATION_LOST);
            sercom_hw->I2CM.STATUS.bit.ARBLOST = 1;  // ack the error
        }

        if (sercom_hw->I2CM.STATUS.bit.LOWTOUT) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::SCL_LOW_TIMEOUT);
            sercom_hw->I2CM.STATUS.bit.LOWTOUT = 1;  // ack the error
        }

        if (sercom_hw->I2CM.STATUS.bit.MEXTTOUT) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::MASTER_SCL_EXTEND_TIMEOUT);
            sercom_hw->I2CM.STATUS.bit.MEXTTOUT = 1;  // ack the error
        }

        if (sercom_hw->I2CM.STATUS.bit.SEXTTOUT) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::SLAVE_SCL_EXTEND_TIMEOUT);
            sercom_hw->I2CM.STATUS.bit.SEXTTOUT = 1;  // ack the error
        }

        if (sercom_hw->I2CM.STATUS.bit.LENERR) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::LENGTH_ERROR);
            sercom_hw->I2CM.STATUS.bit.LENERR = 1;  // ack the error
        }

        // Acknowledge the error flag
        sercom_hw->I2CM.INTFLAG.bit.ERROR = 1;

        // Handle the error
        switch (this->m_state) {
            case State::IDLE:
                // We got an interrupt when we were not expecting to
                this->log_WARNING_HI_UnexpectedInterrupt(this->m_sercom, I2cDriver_I2CInterrupt::BUS_ERROR);
                break;
            case State::READ: {
                auto buf = this->m_read.getBuffer();
                this->m_state = State::IDLE;
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
                if (this->isConnected_readComplete_OutputPort(0)) {
                    this->readComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_READ_ERR);
                }
                break;
            }
            case State::WRITE: {
                auto buf = this->m_write.getBuffer();
                this->m_state = State::IDLE;
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::WRITE);
                if (this->isConnected_writeComplete_OutputPort(0)) {
                    this->writeComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_WRITE_ERR);
                }
                break;
            }
            case State::WRITE_READ_WRITING_WAIT:
            case State::WRITE_READ_WRITING: {
                auto w_buf = this->m_write.getBuffer();
                auto r_buf = this->m_read.getBuffer();
                this->m_state = State::IDLE;
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::WRITE);
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);

                if (this->isConnected_writeReadComplete_OutputPort(0)) {
                    this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_WRITE_ERR);
                }
                break;
            }
            case State::WRITE_READ_READING: {
                auto w_buf = this->m_write.getBuffer();
                auto r_buf = this->m_read.getBuffer();
                this->m_state = State::IDLE;

                // Write already finished, no need to abort
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);

                if (this->isConnected_writeReadComplete_OutputPort(0)) {
                    this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_READ_ERR);
                }
                break;
            }
            default:
                FW_ASSERT(false, this->m_sercom, static_cast<FwAssertArgType>(this->m_state));
        }
    } else {
        // The only other interrupt source should be master on bus
        FW_ASSERT(sercom_hw->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB,
                  static_cast<FwAssertArgType>(sercom_hw->I2CM.INTFLAG.reg));

        // Ack the interrupt
        sercom_hw->I2CM.INTFLAG.bit.MB = 1;

        switch (this->m_state) {
            case State::WRITE_READ_WRITING_WAIT: {
                // We have finished transfering the Tx bytes via DMA and the i2c peripheral
                // entered a MASTER_ON_BUS mode where it is ready to read from the slave.
                sercom_hw->I2CM.INTENCLR.bit.MB = 1;

                // The write of the write/read chain finished
                // Proceed to read.
                this->m_state = State::WRITE_READ_READING;

                auto buf = this->m_read.getBuffer();

                // Read DMA has already been queued, no need to do it again
                this->readImpl(this->m_pending_read_address, buf, /* queueDma */ false);
                break;
            }
            default:
                // We got an interrupt when we were not expecting to
                this->log_WARNING_HI_UnexpectedInterrupt(this->m_sercom, I2cDriver_I2CInterrupt::MASTER_ON_BUS);
                break;
        }
    }
}
// ----------------------------------------------------------------------
// Command handler implementations
// ----------------------------------------------------------------------

void I2cDriver::CLEAR_ERRORS_cmdHandler(FwOpcodeType opCode,  //!< The opcode
                                        U32 cmdSeq            //!< The command sequence number
) {
    this->m_tlmErrors = 0;
    this->cmdResponse_out(opCode, cmdSeq, Fw::CmdResponse::OK);
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

void I2cDriver ::dmaReplyIn_handler(FwIndexType portNum, const Samd21::Dma::Reply& reply) {
    FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK, static_cast<FwAssertArgType>(this->m_sercom),
              static_cast<FwAssertArgType>(this->m_state), portNum);

    // Validate which DMA channel replied to us
    switch (this->m_state) {
        case State::IDLE:
        case State::WRITE_READ_WRITING_WAIT:
            return;

            // Expecting a read reply
        case State::READ:
        case State::WRITE_READ_READING:
            switch (portNum) {
                case I2cDriver_DmaChannel::WRITE:
                    this->log_WARNING_HI_InvalidDmaReply(this->m_sercom, portNum, Samd21::I2cDriver_DmaChannel::READ);
                    return;
                case I2cDriver_DmaChannel::READ:
                    break;
                default:
                    FW_ASSERT(false, portNum);
            }

            break;
        case State::WRITE:
        case State::WRITE_READ_WRITING:
            switch (portNum) {
                case I2cDriver_DmaChannel::WRITE:
                    break;
                case I2cDriver_DmaChannel::READ:
                    this->log_WARNING_HI_InvalidDmaReply(this->m_sercom, portNum, Samd21::I2cDriver_DmaChannel::WRITE);
                    return;
                default:
                    FW_ASSERT(false, portNum);
            }

            break;
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(this->m_state));
    }

    switch (this->m_state) {
        case State::IDLE:
        case State::WRITE_READ_WRITING_WAIT:
            // We already handled these cases
            this->log_WARNING_HI_InvalidDmaReply(this->m_sercom, portNum, Samd21::I2cDriver_DmaChannel::N);
            break;
        case State::READ: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            auto buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;
            if (this->isConnected_readComplete_OutputPort(0)) {
                this->readComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_OK);
            }
            break;
        }
        case State::WRITE: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            auto buf = this->m_write.getBuffer();
            this->m_state = State::IDLE;
            if (this->isConnected_writeComplete_OutputPort(0)) {
                this->writeComplete_out(this->m_portNum, buf, Drv::I2cStatus::I2C_OK);
            }
            break;
        }
        case State::WRITE_READ_WRITING: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);

            // The DMA has finished transfering the Tx packets
            // Because we do not enable LENEN in writeRead (write portion),
            // the I2C peripheral is currently holding the clock until the software
            // generates another condition.
            // In reality we may not have finished transfering the final byte so we
            // need to wait until the MB (master on bus) is set to proceed with the
            // read.

            this->m_state = State::WRITE_READ_WRITING_WAIT;

            // Enable the master on bus interrupt which should trigger once the I2C
            // master is ready to begin the read operation during the clock hold.
            Sercom* sercom_hw = SercomUtil::getHardware(this->m_sercom);
            FW_ASSERT(sercom_hw != nullptr, this->m_sercom);
            sercom_hw->I2CM.INTENSET.bit.MB = 1;
            break;
        }
        case State::WRITE_READ_READING: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            auto w_buf = this->m_write.getBuffer();
            auto r_buf = this->m_read.getBuffer();
            this->m_state = State::IDLE;

            if (this->isConnected_writeReadComplete_OutputPort(0)) {
                this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_OK);
            }
            break;
        }
        default:
            FW_ASSERT(false, this->m_sercom, static_cast<FwAssertArgType>(this->m_state));
    }
}

void I2cDriver ::read_handler(FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    if (this->m_state != State::IDLE) {
        if (this->isConnected_readComplete_OutputPort(portNum)) {
            this->readComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        }
        return;
    }

    this->m_state = State::READ;
    this->m_read = ThinBuffer(buffer);
    this->m_portNum = portNum;

    this->readImpl(addr, buffer, /* queueDma */ true);
}

void I2cDriver ::reportTelemetryIn_handler(FwIndexType portNum, U32 context) {
    auto now = this->getTime();
    Sercom* sercom_hw = SercomUtil::getHardware(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    U8 busStateRaw = sercom_hw->I2CM.STATUS.bit.BUSSTATE;
    U8 clkHoldRaw = sercom_hw->I2CM.STATUS.bit.CLKHOLD;
    U8 rxNackRaw = sercom_hw->I2CM.STATUS.bit.RXNACK;
    U8 slaveOnBusRaw = sercom_hw->I2CM.INTFLAG.bit.SB;
    U8 masterOnBusRaw = sercom_hw->I2CM.INTFLAG.bit.MB;

    I2cDriver_I2cBusState busState;
    I2cDriver_DeviceOnBusFlag deviceOnBus;

    switch (busStateRaw) {  // 2-bits
        case 0x0:
            busState = I2cDriver_I2cBusState::UNKNOWN;
            break;
        case 0x1:
            busState = I2cDriver_I2cBusState::IDLE;
            break;
        case 0x2:
            busState = I2cDriver_I2cBusState::OWNER;
            break;
        case 0x3:
            busState = I2cDriver_I2cBusState::BUSY;
            break;
        default:
            FW_ASSERT(false, busStateRaw);
    }

    if (slaveOnBusRaw != 0 && masterOnBusRaw != 0) {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::MASTER_AND_SLAVE_ON_BUS;
    } else if (slaveOnBusRaw != 0) {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::SLAVE_ON_BUS;
    } else if (masterOnBusRaw != 0) {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::MASTER_ON_BUS;
    } else {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::NONE;
    }

    this->tlmWrite_I2cBusErrorFlags(this->m_tlmErrors, now);
    this->tlmWrite_BusState(busState, now);
    this->tlmWrite_ClockHold(clkHoldRaw != 0, now);
    this->tlmWrite_ReceiveNotAcknowledged(rxNackRaw != 0, now);
    this->tlmWrite_DeviceOnBus(deviceOnBus, now);
}

// I2C DMA is limited to 255 bytes
constexpr U32 SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE = 255;

void I2cDriver::queueReadDma() {
    Sercom* sercom_hw = SercomUtil::getHardware(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    // Queue up a DMA operation to read the data from the device
    this->dmaTransactionOut_out(
        I2cDriver_DmaChannel::READ, SercomUtil::rxDmaTrigger(this->m_sercom), Samd21::Dma::TransactionType::BEAT,
        Samd21::Dma::Priority::PRIORITY_1, reinterpret_cast<U32>(&sercom_hw->I2CM.DATA),
        reinterpret_cast<U32>(this->m_read.getData()), this->m_read.getSize(), Samd21::Dma::BeatSize::BYTE,
        /* incrementSource */ false, /* incrementDestination */ true, Samd21::Dma::AddressIncrementStepSize::SIZE_1,
        Samd21::Dma::StepSelection::DESTINATION);
}

void I2cDriver::queueWriteDma() {
    Sercom* sercom_hw = SercomUtil::getHardware(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    this->dmaTransactionOut_out(
        I2cDriver_DmaChannel::WRITE, SercomUtil::txDmaTrigger(this->m_sercom), Samd21::Dma::TransactionType::BEAT,
        Samd21::Dma::Priority::PRIORITY_1, reinterpret_cast<U32>(this->m_write.getData()),
        reinterpret_cast<U32>(&sercom_hw->I2CM.DATA), this->m_write.getSize(), Samd21::Dma::BeatSize::BYTE,
        /* incrementSource */ true, /* incrementDestination */ false, Samd21::Dma::AddressIncrementStepSize::SIZE_1,
        Samd21::Dma::StepSelection::SOURCE);
}

void I2cDriver::readImpl(U32 addr, Fw::Buffer& buffer, bool queueDma) {
    FW_ASSERT(this->m_configured);

    Sercom* sercom_hw = SercomUtil::getHardware(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    FW_ASSERT(buffer.getData() != nullptr);
    FW_ASSERT(buffer.getSize() <= SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE, buffer.getSize());

    // We only support 7-bit address mode
    FW_ASSERT((addr & ~0x7F) == 0, addr);

    this->log_ACTIVITY_LO_Transaction(this->m_sercom, Samd21::I2cDriver_I2CTransactionKind::READ,
                                      static_cast<U8>(addr));

    // Send ACK to data coming back from the peripheral
    sercom_hw->I2CM.CTRLB.bit.ACKACT = 0;

    if (queueDma) {
        this->queueReadDma();
    }

    SERCOM_I2CM_ADDR_Type addrReg = {};
    addrReg.bit.HS = 0;     // We do not currently support high speed mode
    addrReg.bit.LENEN = 1;  // Enable length mode to generate DMA requests
    addrReg.bit.LEN = static_cast<U8>(buffer.getSize());
    addrReg.bit.ADDR = (addr << 1) | 0x1;  // send a read request

    // Kick off the I2C job by writing the address
    sercom_hw->I2CM.ADDR.reg = addrReg.reg;
}

void I2cDriver::writeImpl(U32 addr, Fw::Buffer& buffer, bool generateStopCondition, bool queueReadDma) {
    FW_ASSERT(this->m_configured);

    Sercom* sercom_hw = SercomUtil::getHardware(this->m_sercom);
    FW_ASSERT(sercom_hw != nullptr, this->m_sercom);

    // I2C DMA is limited to 255 bytes
    FW_ASSERT(buffer.getData() != nullptr);
    FW_ASSERT(buffer.getSize() <= SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE, buffer.getSize());
    FW_ASSERT(buffer.getSize() > 0);

    // We only support 7-bit address mode
    FW_ASSERT((addr & ~0x7F) == 0, addr);

    this->log_ACTIVITY_LO_Transaction(this->m_sercom, Samd21::I2cDriver_I2CTransactionKind::WRITE,
                                      static_cast<U8>(addr));

    // Queue up a DMA operation to write the data to the device
    this->queueWriteDma();

    // Queue up a read DMA if we are doing writeRead
    if (queueReadDma) {
        FW_ASSERT(this->m_read.getData() != nullptr);
        FW_ASSERT(this->m_read.getSize() <= SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE, this->m_read.getSize());
        FW_ASSERT(this->m_read.getSize() > 0);

        this->queueReadDma();
    }

    SERCOM_I2CM_ADDR_Type addrReg = {};
    addrReg.bit.HS = 0;  // We do not currently support high speed mode
    if (generateStopCondition) {
        // Enable length mode to generate an automatic stop condition after
        // the DMA writes all the bytes into DATA
        addrReg.bit.LENEN = 1;
        addrReg.bit.LEN = static_cast<U8>(buffer.getSize());
    } else {
        // Disable length mode to not generate an automatic stop condition
        addrReg.bit.LENEN = 0;
    }

    addrReg.bit.ADDR = (addr << 1) | 0x0;  // send a write request

    // Send NACK if anyone tries to send data to us
    sercom_hw->I2CM.CTRLB.bit.ACKACT = 1;

    // Kick off the I2C job by writing the address
    sercom_hw->I2CM.ADDR.reg = addrReg.reg;
}

void I2cDriver ::write_handler(FwIndexType portNum, U32 addr, Fw::Buffer& buffer) {
    if (this->m_state != State::IDLE) {
        if (this->isConnected_writeComplete_OutputPort(portNum)) {
            this->writeComplete_out(portNum, buffer, Drv::I2cStatus::I2C_OTHER_ERR);
        }
        return;
    }

    this->m_state = State::WRITE;
    this->m_write = ThinBuffer(buffer);
    this->m_portNum = portNum;

    this->writeImpl(addr, buffer, /* generateStopCondition */ true, /* queueReadDma */ false);
}

void I2cDriver ::writeRead_handler(FwIndexType portNum, U32 addr, Fw::Buffer& writeBuffer, Fw::Buffer& readBuffer) {
    if (this->m_state != State::IDLE) {
        if (this->isConnected_writeReadComplete_OutputPort(portNum)) {
            this->writeReadComplete_out(portNum, writeBuffer, readBuffer, Drv::I2cStatus::I2C_OTHER_ERR);
        }
        return;
    }

    this->m_state = State::WRITE_READ_WRITING;
    this->m_read = ThinBuffer(readBuffer);
    this->m_write = ThinBuffer(writeBuffer);
    this->m_portNum = portNum;

    // Stash the address for the read phase: once the write DMA completes,
    // dmaReplyIn_handler issues readImpl(m_pending_read_address, ...). Without this
    // the read half of every write-read would target address 0x00.
    this->m_pending_read_address = addr;

    this->writeImpl(addr, writeBuffer, /* generateStopCondition */ false, /* queueReadDma */ true);
}

}  // namespace Samd21

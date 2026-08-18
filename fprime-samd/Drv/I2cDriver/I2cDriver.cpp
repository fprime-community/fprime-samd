// ======================================================================
// \title  I2cDriver.cpp
// \author tumbar
// \brief  cpp file for I2cDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/I2cDriver/I2cDriverHardware.hpp"
#include "fprime-samd/Drv/Types/CriticalSection.hpp"
#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "fprime-samd/Drv/Types/StatusEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"
#include "samd-config/I2cDriverConfig.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

I2cDriver ::I2cDriver(const char* const compName)
    : I2cDriverComponentBase(compName),
      m_sercom(SercomKind::SERCOM_0),
      m_configured(false),
      m_state(I2cDriver::State::IDLE),
      m_pendingStatus(Drv::I2cStatus::I2C_OK),
      m_portNum(),
      m_read(),
      m_pending_read_address(0),
      m_write(),
      m_tlmErrors(0),
      m_stallTicks(0),
      m_stallRecoveryCount(0) {}

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
    I2cHardware::I2cHal::registerIsr(sercom, I2cDriver::s_i2cDriverIsrHandler, this);

    // Configure and enable the SERCOM I2C host peripheral.
    I2cHardware::I2cHal::configure(sercom, scl_low_timeout, inactive_timeout, clock_stretch_mode, frequency,
                                   client_scl_low_timeout, host_scl_low_timeout, sda_hold, pin_usage, run_in_standby);

    this->m_configured = true;
}

void I2cDriver::s_i2cDriverIsrHandler(Fw::PassiveComponentBase* i2cDriverRaw, SercomKind sercom) {
    auto i2cDriver = reinterpret_cast<I2cDriver*>(i2cDriverRaw);

    FW_ASSERT(i2cDriver != nullptr);
    FW_ASSERT(sercom == i2cDriver->m_sercom, sercom, i2cDriver->m_sercom);
    i2cDriver->isrHandler();
}

void I2cDriver ::isrHandler() {
    // There are two interrupt sources:
    // 1. Errors on the I2C bus
    // 2. Post-Tx ACK from the slave during a writeRead
    //
    // We need to handle both cases in the same handler
    const I2cHardware::I2cInterruptStatus status = I2cHardware::I2cHal::readInterruptStatus(this->m_sercom);

    if (status.error) {
        // The interrupt source was an error
        // The other interrupt source _could_ still be set but we should ack it and ignore it

        // Look up the error(s)
        if (status.busError) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::BUS_ERROR);
        }

        if (status.arbLost) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::ARBITRATION_LOST);
        }

        if (status.lowTimeout) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::SCL_LOW_TIMEOUT);
        }

        if (status.masterExtTimeout) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::MASTER_SCL_EXTEND_TIMEOUT);
        }

        if (status.slaveExtTimeout) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::SLAVE_SCL_EXTEND_TIMEOUT);
        }

        if (status.lengthError) {
            this->m_tlmErrors++;
            this->log_WARNING_LO_I2cBusError(this->m_sercom, Samd21::I2cDriver_I2cError::LENGTH_ERROR);
        }

        // Acknowledge the master-on-bus flag (if set) and all error flags.
        I2cHardware::I2cHal::acknowledgeErrors(this->m_sercom);

        // Handle the error. The DMA channels are aborted here (the transaction
        // must stop immediately), but the client reply is NOT issued from this ISR:
        // the driver moves into the matching COMPLETE_* state and records the
        // status, and the reply is delivered from activeIn in the main context.
        switch (this->m_state) {
            case State::IDLE:
            case State::COMPLETE_READ:
            case State::COMPLETE_WRITE:
            case State::COMPLETE_WRITE_READ:
                // Error IRQ with no in-flight transaction: either genuinely idle,
                // or a completion is already recorded and waiting for activeIn to
                // deliver it. Nothing to abort or re-report; just flag it and leave
                // the pending completion untouched.
                this->log_WARNING_HI_UnexpectedInterrupt(this->m_sercom, I2cDriver_I2CInterrupt::BUS_ERROR);
                break;
            case State::READ:
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
                this->m_pendingStatus = Drv::I2cStatus::I2C_READ_ERR;
                this->m_state = State::COMPLETE_READ;
                break;
            case State::WRITE:
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::WRITE);
                this->m_pendingStatus = Drv::I2cStatus::I2C_WRITE_ERR;
                this->m_state = State::COMPLETE_WRITE;
                break;
            case State::WRITE_READ_WRITING_WAIT:
            case State::WRITE_READ_WRITING:
                // WRITE_READ_WRITING_WAIT has the MB interrupt enabled (armed by the
                // write-DMA completion). Disable it before recovering, otherwise the
                // stale enable leaks into the next transaction and can latch MB
                // against an unexpected state. Harmless in WRITE_READ_WRITING (not
                // yet armed).
                if (this->m_state == State::WRITE_READ_WRITING_WAIT) {
                    I2cHardware::I2cHal::disableMasterOnBusInterrupt(this->m_sercom);
                }

                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::WRITE);
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
                this->m_pendingStatus = Drv::I2cStatus::I2C_WRITE_ERR;
                this->m_state = State::COMPLETE_WRITE_READ;
                break;
            case State::WRITE_READ_READING:
                // Write already finished, no need to abort it
                this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
                this->m_pendingStatus = Drv::I2cStatus::I2C_READ_ERR;
                this->m_state = State::COMPLETE_WRITE_READ;
                break;
            default:
                FW_ASSERT(false, this->m_sercom, static_cast<FwAssertArgType>(this->m_state));
        }
    } else if (!status.masterOnBus) {
        // Woke with neither ERROR nor MB set: a spurious interrupt. This is
        // expected. When a write-read handoff is serviced inline from the write-DMA
        // completion (dmaReplyIn WRITE_READ_WRITING), we enable INTENSET.MB while MB
        // is already latched, which asserts the SERCOM line and latches the NVIC
        // pending bit before we ack MB and disable the interrupt. That stale pending
        // bit still fires one SERCOM interrupt afterwards. MB is already cleared, so
        // there is nothing to do -- any real pending source (ERROR) is level-
        // sensitive and would have re-latched into status above.
        return;
    } else {
        // Master on bus.

        // Ack the interrupt. MB is level-triggered and clearing it here (write-1)
        // prevents it from re-latching this handler; the transaction is advanced by
        // the actions taken below (writing ADDR/DATA), not by the ack itself.
        I2cHardware::I2cHal::acknowledgeMasterOnBus(this->m_sercom);

        switch (this->m_state) {
            case State::WRITE_READ_WRITING_WAIT:
                this->serviceWriteReadMasterOnBus();
                break;
            default:
                // MB fired in a state that does not expect it. Ack (done above) is
                // not enough on its own: leaving the MB interrupt enabled against a
                // stale state would let it re-fire indefinitely, so disable it here.
                // Without this, a single mis-ordered handoff could latch MB with the
                // bus idle and wedge every future write-read (see serviceWriteRead-
                // MasterOnBus / dmaReplyIn WRITE_READ_WRITING).
                I2cHardware::I2cHal::disableMasterOnBusInterrupt(this->m_sercom);
                this->log_WARNING_HI_UnexpectedInterrupt(this->m_sercom, I2cDriver_I2CInterrupt::MASTER_ON_BUS);
                break;
        }
    }
}

void I2cDriver ::serviceWriteReadMasterOnBus() {
    FW_ASSERT(this->m_state == State::WRITE_READ_WRITING_WAIT, static_cast<FwAssertArgType>(this->m_state));

    // We have finished transfering the Tx bytes via DMA and the i2c peripheral
    // entered a MASTER_ON_BUS mode where it is ready to read from the slave.
    // The master-on-bus interrupt has served its purpose; disable it.
    I2cHardware::I2cHal::disableMasterOnBusInterrupt(this->m_sercom);

    // The write phase of the write-read uses LENEN=0, so a client NACK of
    // the address or register-pointer byte does not raise LENERR/ERROR. Check
    // RXNACK here: if the client did not ACK, the write half failed, so abort
    // the pre-armed read DMA and report the write-read as a write error rather
    // than issuing a repeated START into a device that never accepted the pointer.
    if (I2cHardware::I2cHal::readInterruptStatus(this->m_sercom).rxNack) {
        this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
        // Record the write error; activeIn delivers the reply from the main context.
        this->m_pendingStatus = Drv::I2cStatus::I2C_WRITE_ERR;
        this->m_state = State::COMPLETE_WRITE_READ;
        return;
    }

    // The write of the write/read chain finished
    // Proceed to read.
    this->m_state = State::WRITE_READ_READING;

    auto buf = this->m_read.getBuffer();

    // Read DMA has already been queued, no need to do it again
    this->readImpl(this->m_pending_read_address, buf, /* queueDma */ false);
}

void I2cDriver ::recoverFromStall(State stalledState) {
    FW_ASSERT(stalledState != State::IDLE, static_cast<FwAssertArgType>(stalledState));

    // The caller has already atomically set m_state = IDLE under a critical
    // section, so any completion ISR that races in during recovery is dropped by
    // the IDLE guards rather than acting on half-reset state.

    // Capture the frozen registers BEFORE recovery clears them, so the event
    // records the actual wedge signature (e.g. MB latched with BUSSTATE=IDLE).
    const I2cHardware::I2cRawRegisters regs = I2cHardware::I2cHal::readRawRegisters(this->m_sercom);
    this->log_WARNING_HI_StalledTransactionRecovered(this->m_sercom, static_cast<U8>(stalledState), regs.intflag,
                                                     regs.status);

    // Abort whichever DMA channels this transaction could have had in flight.
    switch (stalledState) {
        case State::READ:
            this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
            break;
        case State::WRITE:
            this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::WRITE);
            break;
        case State::WRITE_READ_WRITING:
        case State::WRITE_READ_WRITING_WAIT:
        case State::WRITE_READ_READING:
            this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::WRITE);
            this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
            break;
        default:
            break;
    }

    // Clear all pending flags, disable the MB interrupt, and force the bus to IDLE.
    I2cHardware::I2cHal::recoverBusToIdle(this->m_sercom);

    // Reply to the stuck caller with an error so the client's own state machine
    // unwinds instead of waiting forever. Map to the port that matches the
    // transaction kind, mirroring the ISR error paths.
    Fw::Buffer read = this->m_read.getBuffer();
    Fw::Buffer write = this->m_write.getBuffer();
    switch (stalledState) {
        case State::READ:
            if (this->isConnected_readComplete_OutputPort(this->m_portNum)) {
                this->readComplete_out(this->m_portNum, read, Drv::I2cStatus::I2C_OTHER_ERR);
            }
            break;
        case State::WRITE:
            if (this->isConnected_writeComplete_OutputPort(this->m_portNum)) {
                this->writeComplete_out(this->m_portNum, write, Drv::I2cStatus::I2C_OTHER_ERR);
            }
            break;
        case State::WRITE_READ_WRITING:
        case State::WRITE_READ_WRITING_WAIT:
        case State::WRITE_READ_READING:
            if (this->isConnected_writeReadComplete_OutputPort(this->m_portNum)) {
                this->writeReadComplete_out(this->m_portNum, write, read, Drv::I2cStatus::I2C_OTHER_ERR);
            }
            break;
        default:
            break;
    }

    this->m_stallRecoveryCount++;
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
        case State::COMPLETE_READ:
        case State::COMPLETE_WRITE:
        case State::COMPLETE_WRITE_READ:
            // No DMA is expected in these states: either idle, waiting on the MB
            // interrupt (not a DMA event), or a completion is already recorded and
            // pending delivery. Drop the reply.
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
        case State::COMPLETE_READ:
        case State::COMPLETE_WRITE:
        case State::COMPLETE_WRITE_READ:
            // We already handled these cases
            this->log_WARNING_HI_InvalidDmaReply(this->m_sercom, portNum, Samd21::I2cDriver_DmaChannel::N);
            break;
        case State::READ: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            // Record the successful read; activeIn delivers the reply from the main context.
            this->m_pendingStatus = Drv::I2cStatus::I2C_OK;
            this->m_state = State::COMPLETE_READ;
            break;
        }
        case State::WRITE: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            // Record the successful write; activeIn delivers the reply from the main context.
            this->m_pendingStatus = Drv::I2cStatus::I2C_OK;
            this->m_state = State::COMPLETE_WRITE;
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
            //
            // MB is a LEVEL condition. it is set the moment the write
            // address/data byte is ACKed, which may already be true by the time this
            // DMA-completion ISR runs.
            //
            // The critical section keeps m_state and the MB-interrupt enable
            // consistent as seen by the SERCOM ISR.
            Samd21::CriticalSection cs;

            this->m_state = State::WRITE_READ_WRITING_WAIT;

            // Enable the master on bus interrupt which should trigger once the I2C
            // master is ready to begin the read operation during the clock hold.
            I2cHardware::I2cHal::enableMasterOnBusInterrupt(this->m_sercom);

            // If MB is already latched, no further edge is coming: acknowledge it and
            // service the handoff now. Otherwise the MB interrupt will drive it later.
            if (I2cHardware::I2cHal::readInterruptStatus(this->m_sercom).masterOnBus) {
                I2cHardware::I2cHal::acknowledgeMasterOnBus(this->m_sercom);
                this->serviceWriteReadMasterOnBus();
            }
            break;
        }
        case State::WRITE_READ_READING: {
            FW_ASSERT(reply.get_status() == Samd21::Dma::Status::OK);
            // Record the successful write-read; activeIn delivers the reply from the main context.
            this->m_pendingStatus = Drv::I2cStatus::I2C_OK;
            this->m_state = State::COMPLETE_WRITE_READ;
            break;
        }
        default:
            FW_ASSERT(false, this->m_sercom, static_cast<FwAssertArgType>(this->m_state));
    }
}

bool I2cDriver ::activeIn_handler(FwIndexType portNum, U32 context) {
    // Snapshot the pending completion under a critical section so a completion ISR
    // cannot mutate m_state / m_pendingStatus between the read and the transition
    // back to IDLE. The actual client callback (potentially long) runs OUTSIDE the
    // critical section.
    State completed;
    Drv::I2cStatus status;
    {
        Samd21::CriticalSection cs;
        completed = this->m_state;
        switch (completed) {
            case State::COMPLETE_READ:
            case State::COMPLETE_WRITE:
            case State::COMPLETE_WRITE_READ:
                status = this->m_pendingStatus;
                this->m_state = State::IDLE;
                break;
            default:
                // No completion pending (idle or a transaction still in flight):
                // nothing to deliver this tick.
                return false;
        }
    }

    // Deliver the reply on the port matching the completed transaction kind. The
    // buffers are still owned by the driver until the callback returns them.
    switch (completed) {
        case State::COMPLETE_READ: {
            Fw::Buffer buf = this->m_read.getBuffer();
            if (this->isConnected_readComplete_OutputPort(this->m_portNum)) {
                this->readComplete_out(this->m_portNum, buf, status);
            }
            break;
        }
        case State::COMPLETE_WRITE: {
            Fw::Buffer buf = this->m_write.getBuffer();
            if (this->isConnected_writeComplete_OutputPort(this->m_portNum)) {
                this->writeComplete_out(this->m_portNum, buf, status);
            }
            break;
        }
        case State::COMPLETE_WRITE_READ: {
            Fw::Buffer w_buf = this->m_write.getBuffer();
            Fw::Buffer r_buf = this->m_read.getBuffer();
            if (this->isConnected_writeReadComplete_OutputPort(this->m_portNum)) {
                this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, status);
            }
            break;
        }
        default:
            FW_ASSERT(false, static_cast<FwAssertArgType>(completed));
    }

    return true;
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

    // Stall watchdog. A healthy transaction completes in milliseconds, well within
    // a single telemetry tick. If the driver is still non-IDLE across
    // I2C_STALL_RECOVERY_TICKS consecutive ticks, its completion interrupt was lost
    // (e.g. a hardware time-out auto-STOPped the bus but the completion never
    // propagated) and it will otherwise stay wedged forever. Force-recover it.
    //
    // The transaction is *claimed* under a critical section so a completion ISR
    // cannot drive m_state to IDLE between the test and the claim: either the ISR
    // wins and we observe IDLE (counter resets), or we win, atomically capture the
    // stuck state and set m_state = IDLE. The actual recovery (event, DMA aborts,
    // HAL reset, port replies -- all potentially long) then runs OUTSIDE the
    // critical section so interrupts are not disabled across downstream calls. Any
    // ISR that races in after the claim sees IDLE and is dropped by the IDLE guards.
    State stalledState = State::IDLE;
    {
        Samd21::CriticalSection cs;
        // A COMPLETE_* state is not a stall: the transaction already finished in
        // the ISR, the bus is idle, and the reply is only waiting for the next
        // activeIn tick. Only the in-flight states (READ/WRITE/WRITE_READ_*) can
        // wedge, so only they accrue stall ticks.
        const bool inFlight = (this->m_state != State::IDLE) && (this->m_state != State::COMPLETE_READ) &&
                              (this->m_state != State::COMPLETE_WRITE) && (this->m_state != State::COMPLETE_WRITE_READ);
        if (!inFlight) {
            this->m_stallTicks = 0;
        } else {
            this->m_stallTicks++;
            if (this->m_stallTicks >= I2cDriverConfig::I2C_STALL_RECOVERY_TICKS) {
                stalledState = this->m_state;
                this->m_state = State::IDLE;
                this->m_stallTicks = 0;
            }
        }
    }
    if (stalledState != State::IDLE) {
        this->recoverFromStall(stalledState);
    }

    // The bus-error counter is always emitted -- it is the driver's health signal.
    this->tlmWrite_BusErrorCount(this->m_tlmErrors, now);
    this->tlmWrite_StallRecoveryCount(this->m_stallRecoveryCount, now);

    // The remaining diagnostic channels are compile-time gated. When disabled we
    // also skip the hardware bus-status read they depend on.
    if (!I2cDriverConfig::I2C_ENABLE_DEBUG_TELEMETRY) {
        return;
    }

    const I2cHardware::I2cBusStatus status = I2cHardware::I2cHal::readBusStatus(this->m_sercom);

    I2cDriver_I2cBusState busState;
    I2cDriver_DeviceOnBusFlag deviceOnBus;

    switch (status.busState) {  // 2-bits
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
            FW_ASSERT(false, status.busState);
    }

    if (status.slaveOnBus && status.masterOnBus) {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::MASTER_AND_SLAVE_ON_BUS;
    } else if (status.slaveOnBus) {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::SLAVE_ON_BUS;
    } else if (status.masterOnBus) {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::MASTER_ON_BUS;
    } else {
        deviceOnBus = I2cDriver_DeviceOnBusFlag::NONE;
    }

    this->tlmWrite_BusState(busState, now);
    this->tlmWrite_ClockHold(status.clockHold, now);
    this->tlmWrite_ReceiveNotAcknowledged(status.rxNack, now);
    this->tlmWrite_DeviceOnBus(deviceOnBus, now);
}

// I2C DMA is limited to 255 bytes
constexpr U32 SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE = 255;

void I2cDriver::queueReadDma() {
    // Queue up a DMA operation to read the data from the device
    this->dmaTransactionOut_out(I2cDriver_DmaChannel::READ, SercomUtil::rxDmaTrigger(this->m_sercom),
                                Samd21::Dma::TransactionType::BEAT, Samd21::Dma::Priority::PRIORITY_1,
                                I2cHardware::I2cHal::getDataRegisterAddress(this->m_sercom),
                                static_cast<U32>(reinterpret_cast<uintptr_t>(this->m_read.getData())),
                                this->m_read.getSize(), Samd21::Dma::BeatSize::BYTE,
                                /* incrementSource */ false, /* incrementDestination */ true,
                                Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::DESTINATION);
}

void I2cDriver::queueWriteDma() {
    this->dmaTransactionOut_out(I2cDriver_DmaChannel::WRITE, SercomUtil::txDmaTrigger(this->m_sercom),
                                Samd21::Dma::TransactionType::BEAT, Samd21::Dma::Priority::PRIORITY_1,
                                static_cast<U32>(reinterpret_cast<uintptr_t>(this->m_write.getData())),
                                I2cHardware::I2cHal::getDataRegisterAddress(this->m_sercom), this->m_write.getSize(),
                                Samd21::Dma::BeatSize::BYTE,
                                /* incrementSource */ true, /* incrementDestination */ false,
                                Samd21::Dma::AddressIncrementStepSize::SIZE_1, Samd21::Dma::StepSelection::SOURCE);
}

void I2cDriver::readImpl(U32 addr, Fw::Buffer& buffer, bool queueDma) {
    FW_ASSERT(this->m_configured);

    FW_ASSERT(buffer.getData() != nullptr);
    FW_ASSERT(buffer.getSize() <= SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE, buffer.getSize());

    // We only support 7-bit address mode
    FW_ASSERT((addr & ~0x7F) == 0, addr);

    if (queueDma) {
        this->queueReadDma();
    }

    // Kick off the I2C read: sets the read ACK action and writes ADDR with
    // length mode enabled so the DMA drives the transfer.
    I2cHardware::I2cHal::beginRead(this->m_sercom, addr, static_cast<U8>(buffer.getSize()));
}

void I2cDriver::writeImpl(U32 addr, Fw::Buffer& buffer, bool generateStopCondition, bool queueReadDma) {
    FW_ASSERT(this->m_configured);

    // I2C DMA is limited to 255 bytes
    FW_ASSERT(buffer.getData() != nullptr);
    FW_ASSERT(buffer.getSize() <= SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE, buffer.getSize());
    FW_ASSERT(buffer.getSize() > 0);

    // We only support 7-bit address mode
    FW_ASSERT((addr & ~0x7F) == 0, addr);

    // Queue up a DMA operation to write the data to the device
    this->queueWriteDma();

    // Queue up a read DMA if we are doing writeRead
    if (queueReadDma) {
        FW_ASSERT(this->m_read.getData() != nullptr);
        FW_ASSERT(this->m_read.getSize() <= SAMD21_I2C_MAX_DMA_PAYLOAD_SIZE, this->m_read.getSize());
        FW_ASSERT(this->m_read.getSize() > 0);

        this->queueReadDma();
    }

    // Kick off the I2C write. When generateStopCondition is set the peripheral
    // emits an automatic STOP after the DMA writes all the bytes; otherwise the
    // clock is held for a repeated START (write-read).
    I2cHardware::I2cHal::beginWrite(this->m_sercom, addr, static_cast<U8>(buffer.getSize()), generateStopCondition);
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

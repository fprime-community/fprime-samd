// ======================================================================
// \title  I2cDriver.cpp
// \author tumbar
// \brief  cpp file for I2cDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriver.hpp"
#include "Fw/Types/Assert.hpp"
#include "config/FwAssertArgTypeAliasAc.h"
#include "fprime-samd/Drv/I2cDriver/I2cDriverHardware.hpp"
#include "fprime-samd/Drv/Types/Sercom.hpp"
#include "fprime-samd/Drv/Types/StatusEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"

namespace Samd21 {

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
        FW_ASSERT(status.masterOnBus, static_cast<FwAssertArgType>(this->m_state));

        // Ack the interrupt
        I2cHardware::I2cHal::acknowledgeMasterOnBus(this->m_sercom);

        switch (this->m_state) {
            case State::WRITE_READ_WRITING_WAIT: {
                // We have finished transfering the Tx bytes via DMA and the i2c peripheral
                // entered a MASTER_ON_BUS mode where it is ready to read from the slave.
                // Either way the master-on-bus interrupt has served its purpose.
                I2cHardware::I2cHal::disableMasterOnBusInterrupt(this->m_sercom);

                // The write phase of the write-read uses LENEN=0, so a client NACK of
                // the address or register-pointer byte does not raise LENERR/ERROR. Check
                // RXNACK here: if the client did not ACK, the write half failed, so abort
                // the pre-armed read DMA and report the write-read as a write error rather
                // than issuing a repeated START into a device that never accepted the pointer.
                if (status.rxNack) {
                    auto w_buf = this->m_write.getBuffer();
                    auto r_buf = this->m_read.getBuffer();
                    this->m_state = State::IDLE;
                    this->dmaTransactionAbortOut_out(I2cDriver_DmaChannel::READ);
                    if (this->isConnected_writeReadComplete_OutputPort(0)) {
                        this->writeReadComplete_out(this->m_portNum, w_buf, r_buf, Drv::I2cStatus::I2C_WRITE_ERR);
                    }
                    break;
                }

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
            I2cHardware::I2cHal::enableMasterOnBusInterrupt(this->m_sercom);
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

    this->tlmWrite_I2cBusErrorFlags(this->m_tlmErrors, now);
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

    this->log_ACTIVITY_LO_Transaction(this->m_sercom, Samd21::I2cDriver_I2CTransactionKind::READ,
                                      static_cast<U8>(addr));

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

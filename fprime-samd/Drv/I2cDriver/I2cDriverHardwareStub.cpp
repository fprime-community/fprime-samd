// ======================================================================
// \title  I2cDriverHardwareStub.cpp
// \author tumbar
// \brief  Stub hardware implementation for the I2C peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, I2cDriverHardware.cpp is used instead.
//
// The stub records the arguments of each hardware call and exposes injectable
// interrupt/bus status so unit tests can drive the component's state machine,
// ISR error dispatch and telemetry mapping without any real MCU.
// ======================================================================

#include "fprime-samd/Drv/I2cDriver/I2cDriverHardware.hpp"

namespace Samd21 {
namespace I2cHardware {

static StubState s_state = {};

StubState& getStubState() {
    return s_state;
}

void resetStubState() {
    // Assign a fresh value-initialized instance rather than memset: several
    // members (e.g. SercomKind) are autocoded classes with vtables that
    // memset would corrupt.
    s_state = StubState{};
}

void fireIsr() {
    if (s_state.isr_callback != nullptr) {
        s_state.isr_callback(s_state.isr_data, s_state.isr_sercom);
    }
}

void I2cHal::registerIsr(SercomKind sercom,
                         void (*callback)(Fw::PassiveComponentBase*, SercomKind),
                         Fw::PassiveComponentBase* data) {
    s_state.isr_callback = callback;
    s_state.isr_data = data;
    s_state.isr_sercom = sercom;
}

void I2cHal::configure(SercomKind sercom,
                       I2cDriver::SclLowTimeout scl_low_timeout,
                       I2cDriver::InactiveTimeout inactive_timeout,
                       I2cDriver::ClockStretchMode clock_stretch_mode,
                       I2cDriver::Frequency frequency,
                       I2cDriver::ClientSclLowTimeout client_scl_low_timeout,
                       I2cDriver::HostSclLowTimeout host_scl_low_timeout,
                       I2cDriver::SdaHold sda_hold,
                       I2cDriver::PinUsage pin_usage,
                       I2cDriver::RunInStandby run_in_standby) {
    s_state.configured = true;
    s_state.configure_count++;
    s_state.sercom = sercom;
    s_state.scl_low_timeout = scl_low_timeout;
    s_state.inactive_timeout = inactive_timeout;
    s_state.clock_stretch_mode = clock_stretch_mode;
    s_state.frequency = frequency;
    s_state.client_scl_low_timeout = client_scl_low_timeout;
    s_state.host_scl_low_timeout = host_scl_low_timeout;
    s_state.sda_hold = sda_hold;
    s_state.pin_usage = pin_usage;
    s_state.run_in_standby = run_in_standby;
}

U32 I2cHal::getDataRegisterAddress(SercomKind sercom) {
    return s_state.data_register_address;
}

I2cInterruptStatus I2cHal::readInterruptStatus(SercomKind sercom) {
    return s_state.interrupt_status;
}

I2cBusStatus I2cHal::readBusStatus(SercomKind sercom) {
    return s_state.bus_status;
}

I2cRawRegisters I2cHal::readRawRegisters(SercomKind sercom) {
    return s_state.raw_registers;
}

void I2cHal::recoverBusToIdle(SercomKind sercom) {
    s_state.recover_bus_count++;

    // Model the write-1-clear recovery: after recovery no interrupt/error flags
    // remain pending and the bus is IDLE, so the driver can start fresh.
    s_state.interrupt_status = I2cInterruptStatus{};
    s_state.bus_status.busState = 0x1;  // IDLE
    s_state.bus_status.clockHold = false;
    s_state.bus_status.masterOnBus = false;
    s_state.bus_status.slaveOnBus = false;
}

void I2cHal::acknowledgeErrors(SercomKind sercom) {
    s_state.acknowledge_errors_count++;
    // Model the write-1-clear: once acked, the pending error/flags go away so a
    // subsequent readInterruptStatus() does not re-report the same error.
    s_state.interrupt_status.error = false;
    s_state.interrupt_status.busError = false;
    s_state.interrupt_status.arbLost = false;
    s_state.interrupt_status.lowTimeout = false;
    s_state.interrupt_status.masterExtTimeout = false;
    s_state.interrupt_status.slaveExtTimeout = false;
    s_state.interrupt_status.lengthError = false;
}

void I2cHal::acknowledgeMasterOnBus(SercomKind sercom) {
    s_state.acknowledge_mb_count++;
    s_state.interrupt_status.masterOnBus = false;
}

void I2cHal::enableMasterOnBusInterrupt(SercomKind sercom) {
    s_state.enable_mb_count++;
}

void I2cHal::disableMasterOnBusInterrupt(SercomKind sercom) {
    s_state.disable_mb_count++;
}

void I2cHal::beginRead(SercomKind sercom, U32 addr, U8 byteCount) {
    s_state.begin_read_count++;
    s_state.begin_read_addr = addr;
    s_state.begin_read_len = byteCount;
}

void I2cHal::beginWrite(SercomKind sercom, U32 addr, U8 byteCount, bool generateStopCondition) {
    s_state.begin_write_count++;
    s_state.begin_write_addr = addr;
    s_state.begin_write_len = byteCount;
    s_state.begin_write_stop = generateStopCondition;
}

}  // namespace I2cHardware
}  // namespace Samd21

// ======================================================================
// \title  AdcDriverHardwareStub.cpp
// \author crsmith
// \brief  Stub hardware implementation for the ADC peripheral (Linux/test builds)
//
// This file is compiled for Linux/test builds to enable unit testing.
// For SAMD21 target builds, AdcDriverHardware.cpp would be used instead.
//
// The stub records the arguments of each hardware call and exposes injectable
// hardware state so unit tests can drive the component's logic without any real MCU.
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/AdcDriverHardware.hpp"

namespace Samd21 {
namespace AdcHardware {

static StubState s_state = {};

StubState& getStubState() {
    return s_state;
}

void resetStubState() {
    s_state = StubState{};
}

void AdcHal::waitForGclkSync() {
    s_state.waitForGclkSync_calls++;
    // Stub never blocks - assumes hardware sync completes immediately
}

void AdcHal::waitForAdcSync() {
    s_state.waitForAdcSync_calls++;
    // Stub never blocks - assumes hardware sync completes immediately
}

void AdcHal::configure(AdcDriver::VoltageReference ref,
                        AdcDriver::Resolution res,
                        AdcDriver::SampleCount samples,
                        U8 samplingTime,
                        AdcDriver::Gain gain) {
    // Call sync waits to track them (mirroring the real hardware sequence)
    waitForGclkSync();  // Called once for GCLK setup
    waitForAdcSync();   // Called for reset
    waitForAdcSync();   // Called for REFCTRL
    waitForAdcSync();   // Called for CTRLB
    waitForAdcSync();   // Called for INPUTCTRL before dummy conversion
    // Additional waits happen in the real implementation

    s_state.configured = true;
    s_state.configure_count++;
    s_state.ref = ref;
    s_state.res = res;
    s_state.samples = samples;
    s_state.samplingTime = samplingTime;
    s_state.gain = gain;
}

void AdcHal::configureChannel(AdcDriver::AdcChannel channel) {
    s_state.configureChannel_count++;
    s_state.last_configured_channel = channel;
}

void AdcHal::selectChannel(AdcDriver::AdcChannel channel, U8 gain) {
    s_state.selectChannel_count++;
    s_state.selected_channel = channel;
    s_state.selected_gain = gain;
}

void AdcHal::startConversion() {
    s_state.startConversion_count++;
}

U32 AdcHal::readResult() {
    // Model the auto-clear behavior: reading result clears RESRDY flag
    // per SAMD21 datasheet §33.6.5
    U32 value = s_state.result;
    s_state.result_ready = false;
    return value;
}

bool AdcHal::isResultReady() {
    return s_state.result_ready;
}

bool AdcHal::isOverrun() {
    return s_state.overrun;
}

void AdcHal::clearOverrun() {
    s_state.clearOverrun_count++;
    // Model the write-1-clear: once cleared, no longer set
    s_state.overrun = false;
}

void AdcHal::enableInterrupt() {
    // Stub no-op: interrupt enable recorded implicitly
}

void AdcHal::enableAdcInterrupts() {
    // Stub no-op: peripheral interrupt enable recorded implicitly
}

}  // namespace AdcHardware
}  // namespace Samd21

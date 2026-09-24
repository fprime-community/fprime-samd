// ======================================================================
// \title  GpioDriverTester.cpp
// \author tumbar
// \brief  cpp file for GpioDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/test/ut/GpioDriverTester.hpp"
#include "Fw/Test/UnitTest.hpp"
#include "Fw/Types/LogicEnumAc.hpp"
#include "STest/Pick/Pick.hpp"

namespace Samd21 {

// Construction and destruction

GpioDriverTester::GpioDriverTester()
    : GpioDriverGTestBase("GpioDriverTester", GpioDriverTester::MAX_HISTORY_SIZE), component("GpioDriver") {
    this->initComponents();
    this->connectPorts();
}

GpioDriverTester::~GpioDriverTester() {}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void GpioDriverTester::resetTest() {
    this->clearHistory();
    GpioHardware::resetGpioState();
}

void GpioDriverTester::configureInputAndAssert(GpioDriver::Group group,
                                               GpioDriver::Pin pin,
                                               GpioDriver::InputPullMode input_pull_mode) {
    const GpioHardware::GpioState& state = GpioHardware::getGpioState();
    const U32 before = state.configure_input_count;

    this->component.configureInput(group, pin, input_pull_mode, GpioDriver::ExternalInterruptMode::NONE);

    // The driver must forward exactly one configureInput() call to the HAL,
    // passing each argument through unmodified.
    ASSERT_EQ(state.configure_input_count, before + 1);
    ASSERT_EQ(state.last_group, static_cast<U8>(group));
    ASSERT_EQ(state.last_pin, static_cast<U8>(pin));
    ASSERT_EQ(state.last_input_pull_mode, input_pull_mode);
}

void GpioDriverTester::configureOutputAndAssert(GpioDriver::Group group, GpioDriver::Pin pin) {
    const GpioHardware::GpioState& state = GpioHardware::getGpioState();
    const U32 before = state.configure_output_count;

    this->component.configureOutput(group, pin);

    // The driver must forward exactly one configureOutput() call to the HAL,
    // passing each argument through unmodified.
    ASSERT_EQ(state.configure_output_count, before + 1);
    ASSERT_EQ(state.last_group, static_cast<U8>(group));
    ASSERT_EQ(state.last_pin, static_cast<U8>(pin));
}

void GpioDriverTester::invokeWriteAndAssertStatus(const Fw::Logic& logic, Drv::GpioStatus expected) {
    const GpioHardware::GpioState& state = GpioHardware::getGpioState();
    const U32 before = state.write_count;

    const Drv::GpioStatus status = this->invoke_to_gpioWrite(PORT_NUM, logic);
    ASSERT_EQ(status, expected);

    if (expected == Drv::GpioStatus::OP_OK) {
        // A successful write reaches the HAL exactly once with the logic level.
        ASSERT_EQ(state.write_count, before + 1);
        ASSERT_EQ(state.last_write_state, logic);
    } else {
        // Any error path must not touch the hardware.
        ASSERT_EQ(state.write_count, before);
    }
}

void GpioDriverTester::invokeReadAndAssertStatus(Drv::GpioStatus expected) {
    const GpioHardware::GpioState& state = GpioHardware::getGpioState();
    const U32 before = state.read_count;

    Fw::Logic logic(Fw::Logic::LOW);
    const Drv::GpioStatus status = this->invoke_to_gpioRead(PORT_NUM, logic);
    ASSERT_EQ(status, expected);

    if (expected == Drv::GpioStatus::OP_OK) {
        // A successful read pulls exactly one value out of the HAL and
        // returns it through the ref parameter.
        ASSERT_EQ(state.read_count, before + 1);
        ASSERT_EQ(logic, state.read_value);
    } else {
        // Any error path must not touch the hardware.
        ASSERT_EQ(state.read_count, before);
    }
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void GpioDriverTester::testConfigureOutput() {
    REQUIREMENT(
        "GPIO-001: configureOutput shall bind the instance to one group/pin and forward the "
        "configuration to the PORT peripheral");
    this->resetTest();

    this->configureOutputAndAssert(GpioDriver::Group::PA, GpioDriver::Pin::PIN_5);
}

void GpioDriverTester::testConfigureInput() {
    REQUIREMENT(
        "GPIO-001: configureInput shall bind the instance to one group/pin and forward the "
        "configuration to the PORT peripheral");
    REQUIREMENT(
        "GPIO-002: configureInput shall select a pull-up, pull-down, or no internal resistor via "
        "InputPullMode");

    // Pull-up input
    this->resetTest();
    this->configureInputAndAssert(GpioDriver::Group::PB, GpioDriver::Pin::PIN_10, GpioDriver::InputPullMode::PULL_UP);

    // Pull-down input (fresh component: configure may only be called once)
    GpioDriver comp2("GpioDriver2");
    GpioHardware::resetGpioState();
    comp2.configureInput(GpioDriver::Group::PA, GpioDriver::Pin::PIN_3, GpioDriver::InputPullMode::PULL_DOWN,
                         GpioDriver::ExternalInterruptMode::NONE);
    const GpioHardware::GpioState& s2 = GpioHardware::getGpioState();
    ASSERT_EQ(s2.configure_input_count, 1U);
    ASSERT_EQ(s2.last_input_pull_mode, GpioDriver::InputPullMode::PULL_DOWN);

    // Floating input (no pull)
    GpioDriver comp3("GpioDriver3");
    GpioHardware::resetGpioState();
    comp3.configureInput(GpioDriver::Group::PB, GpioDriver::Pin::PIN_0, GpioDriver::InputPullMode::NO_PULL,
                         GpioDriver::ExternalInterruptMode::NONE);
    const GpioHardware::GpioState& s3 = GpioHardware::getGpioState();
    ASSERT_EQ(s3.configure_input_count, 1U);
    ASSERT_EQ(s3.last_input_pull_mode, GpioDriver::InputPullMode::NO_PULL);
}

void GpioDriverTester::testConfigureAllPins() {
    REQUIREMENT("GPIO-001: configureOutput shall bind the instance to any one group/pin combination");

    const GpioDriver::Group groups[] = {GpioDriver::Group::PA, GpioDriver::Group::PB};

    for (const auto group : groups) {
        for (U8 pinIdx = 0; pinIdx <= static_cast<U8>(GpioDriver::Pin::PIN_31); pinIdx++) {
            // Each configure call must run against a fresh component
            // because the driver asserts configuration happens exactly once.
            GpioDriver comp("GpioDriverPin");
            GpioHardware::resetGpioState();

            const GpioDriver::Pin pin = static_cast<GpioDriver::Pin>(pinIdx);
            comp.configureOutput(group, pin);

            const GpioHardware::GpioState& state = GpioHardware::getGpioState();
            ASSERT_EQ(state.configure_output_count, 1U);
            ASSERT_EQ(state.last_group, static_cast<U8>(group));
            ASSERT_EQ(state.last_pin, pinIdx);
        }
    }
}

void GpioDriverTester::testWriteNominal() {
    REQUIREMENT(
        "GPIO-003: gpioWrite shall set the pin logic level and return OP_OK when the pin is configured "
        "as an output");
    this->resetTest();

    this->component.configureOutput(GpioDriver::Group::PA, GpioDriver::Pin::PIN_7);

    // Both logic levels succeed and are forwarded to the HAL.
    this->invokeWriteAndAssertStatus(Fw::Logic::HIGH, Drv::GpioStatus::OP_OK);
    this->invokeWriteAndAssertStatus(Fw::Logic::LOW, Drv::GpioStatus::OP_OK);
}

void GpioDriverTester::testReadNominal() {
    REQUIREMENT(
        "GPIO-004: gpioRead shall return the pin logic level and OP_OK when the pin is configured as "
        "an input");
    this->resetTest();

    this->component.configureInput(GpioDriver::Group::PB, GpioDriver::Pin::PIN_2, GpioDriver::InputPullMode::PULL_UP,
                                   GpioDriver::ExternalInterruptMode::NONE);

    // The value read back must match whatever the HAL reports.
    GpioHardware::setReadValue(Fw::Logic::HIGH);
    this->invokeReadAndAssertStatus(Drv::GpioStatus::OP_OK);

    GpioHardware::setReadValue(Fw::Logic::LOW);
    this->invokeReadAndAssertStatus(Drv::GpioStatus::OP_OK);
}

void GpioDriverTester::testWriteUnconfigured() {
    REQUIREMENT("GPIO-005: gpioWrite shall return NOT_OPENED if invoked before configureOutput");
    this->resetTest();

    // No configure() call has happened on this component.
    this->invokeWriteAndAssertStatus(Fw::Logic::HIGH, Drv::GpioStatus::NOT_OPENED);
}

void GpioDriverTester::testReadUnconfigured() {
    REQUIREMENT("GPIO-005: gpioRead shall return NOT_OPENED if invoked before configureInput");
    this->resetTest();

    this->invokeReadAndAssertStatus(Drv::GpioStatus::NOT_OPENED);
}

void GpioDriverTester::testWriteWrongMode() {
    REQUIREMENT("GPIO-006: gpioWrite on an input pin shall return INVALID_MODE without touching hardware");
    this->resetTest();

    this->component.configureInput(GpioDriver::Group::PA, GpioDriver::Pin::PIN_1, GpioDriver::InputPullMode::NO_PULL,
                                   GpioDriver::ExternalInterruptMode::NONE);

    this->invokeWriteAndAssertStatus(Fw::Logic::HIGH, Drv::GpioStatus::INVALID_MODE);
}

void GpioDriverTester::testReadWrongMode() {
    REQUIREMENT("GPIO-006: gpioRead on an output pin shall return INVALID_MODE without touching hardware");
    this->resetTest();

    this->component.configureOutput(GpioDriver::Group::PB, GpioDriver::Pin::PIN_4);

    this->invokeReadAndAssertStatus(Drv::GpioStatus::INVALID_MODE);
}

void GpioDriverTester::testConfigureInputExternalInterrupt() {
    REQUIREMENT(
        "GPIO-007: configureInput shall, when ExternalInterruptMode is not NONE, configure the EIC to "
        "detect the selected edge(s) on the pin's EXTINT line");
    this->resetTest();

    // NONE must not touch the EIC.
    this->component.configureInput(GpioDriver::Group::PA, GpioDriver::Pin::PIN_6, GpioDriver::InputPullMode::NO_PULL,
                                   GpioDriver::ExternalInterruptMode::NONE);
    const GpioHardware::GpioState& stateNone = GpioHardware::getGpioState();
    ASSERT_EQ(stateNone.configure_external_interrupt_count, 0U);

    // Each non-NONE mode must reach the HAL exactly once with the mode forwarded unmodified.
    const GpioDriver::ExternalInterruptMode modes[] = {
        GpioDriver::ExternalInterruptMode::RISING,
        GpioDriver::ExternalInterruptMode::FALLING,
        GpioDriver::ExternalInterruptMode::BOTH,
    };

    for (const auto mode : modes) {
        GpioDriver comp("GpioDriverInterrupt");
        GpioHardware::resetGpioState();

        comp.configureInput(GpioDriver::Group::PB, GpioDriver::Pin::PIN_9, GpioDriver::InputPullMode::PULL_UP, mode);

        const GpioHardware::GpioState& state = GpioHardware::getGpioState();
        ASSERT_EQ(state.configure_external_interrupt_count, 1U);
        ASSERT_EQ(state.last_group, static_cast<U8>(GpioDriver::Group::PB));
        ASSERT_EQ(state.last_pin, static_cast<U8>(GpioDriver::Pin::PIN_9));
        ASSERT_EQ(state.last_input_interrupt_mode, mode);
    }
}

void GpioDriverTester::testInterruptFiresWhenConnected() {
    REQUIREMENT(
        "GPIO-008: On each configured edge, the driver shall emit a cycle on the gpioInterrupt output "
        "port when it is connected");
    this->resetTest();

    const GpioDriver::Pin pin = GpioDriver::Pin::PIN_11;
    this->component.configureInput(GpioDriver::Group::PA, pin, GpioDriver::InputPullMode::NO_PULL,
                                   GpioDriver::ExternalInterruptMode::BOTH);

    ASSERT_from_gpioInterrupt_SIZE(0);

    // gpioInterrupt is auto-connected to this tester's recording port by connectPorts(),
    // so simulating an edge on the configured pin must dispatch to it exactly once.
    GpioHardware::simulateInterrupt(static_cast<U8>(pin));
    ASSERT_from_gpioInterrupt_SIZE(1);

    // A second edge dispatches a second cycle.
    GpioHardware::simulateInterrupt(static_cast<U8>(pin));
    ASSERT_from_gpioInterrupt_SIZE(2);
}

void GpioDriverTester::testInterruptIsrNoOpWhenDisconnected() {
    REQUIREMENT(
        "GPIO-008: the driver shall emit a cycle on the gpioInterrupt output port only when it is "
        "connected");
    // A standalone GpioDriver instance's gpioInterrupt port is never connected
    // (only this->component is wired to a recorder via connectPorts()), so its
    // ISR hook must be a safe no-op rather than touch an unconnected port.
    this->resetTest();

    GpioDriver comp("GpioDriverDisconnected");
    comp.configureInput(GpioDriver::Group::PB, GpioDriver::Pin::PIN_15, GpioDriver::InputPullMode::NO_PULL,
                        GpioDriver::ExternalInterruptMode::RISING);

    // Must not crash, and must not affect this->component's (unrelated) history.
    comp.gpioInterruptIsr();

    ASSERT_from_gpioInterrupt_SIZE(0);
}

void GpioDriverTester::testInterruptNoDispatchWithoutRegisteredHandler() {
    REQUIREMENT(
        "GPIO-008: the driver shall emit a cycle on the gpioInterrupt output port only for edges on "
        "the pin it configured");
    // resetTest() clears the HAL's interrupt-handler table; simulating an edge
    // on a pin nothing has registered for must dispatch to nothing.
    this->resetTest();

    GpioHardware::simulateInterrupt(static_cast<U8>(GpioDriver::Pin::PIN_20));

    ASSERT_from_gpioInterrupt_SIZE(0);
}

}  // namespace Samd21

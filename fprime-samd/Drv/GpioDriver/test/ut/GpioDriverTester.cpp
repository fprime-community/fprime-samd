// ======================================================================
// \title  GpioDriverTester.cpp
// \author tumbar
// \brief  cpp file for GpioDriver test harness implementation class
// ======================================================================

#include "fprime-samd/Drv/GpioDriver/test/ut/GpioDriverTester.hpp"
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
    // REQUIREMENT("GPIO-001: configureOutput() shall forward output configuration to the HAL");
    this->resetTest();

    this->configureOutputAndAssert(GpioDriver::Group::PA, GpioDriver::Pin::PIN_5);
}

void GpioDriverTester::testConfigureInput() {
    // REQUIREMENT("GPIO-002: configureInput() shall forward input configuration and pull selection to the HAL");

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
    // REQUIREMENT("GPIO-001: configureOutput() shall support every group/pin combination");

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
    // REQUIREMENT("GPIO-003: gpioWrite on a configured output pin shall drive the HAL and return OP_OK");
    this->resetTest();

    this->component.configureOutput(GpioDriver::Group::PA, GpioDriver::Pin::PIN_7);

    // Both logic levels succeed and are forwarded to the HAL.
    this->invokeWriteAndAssertStatus(Fw::Logic::HIGH, Drv::GpioStatus::OP_OK);
    this->invokeWriteAndAssertStatus(Fw::Logic::LOW, Drv::GpioStatus::OP_OK);
}

void GpioDriverTester::testReadNominal() {
    // REQUIREMENT("GPIO-004: gpioRead on a configured input pin shall read the HAL and return OP_OK");
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
    // REQUIREMENT("GPIO-005: gpioWrite before configure() shall return NOT_OPENED");
    this->resetTest();

    // No configure() call has happened on this component.
    this->invokeWriteAndAssertStatus(Fw::Logic::HIGH, Drv::GpioStatus::NOT_OPENED);
}

void GpioDriverTester::testReadUnconfigured() {
    // REQUIREMENT("GPIO-005: gpioRead before configure() shall return NOT_OPENED");
    this->resetTest();

    this->invokeReadAndAssertStatus(Drv::GpioStatus::NOT_OPENED);
}

void GpioDriverTester::testWriteWrongMode() {
    // REQUIREMENT("GPIO-006: gpioWrite on an input pin shall return INVALID_MODE");
    this->resetTest();

    this->component.configureInput(GpioDriver::Group::PA, GpioDriver::Pin::PIN_1, GpioDriver::InputPullMode::NO_PULL,
                                   GpioDriver::ExternalInterruptMode::NONE);

    this->invokeWriteAndAssertStatus(Fw::Logic::HIGH, Drv::GpioStatus::INVALID_MODE);
}

void GpioDriverTester::testReadWrongMode() {
    // REQUIREMENT("GPIO-006: gpioRead on an output pin shall return INVALID_MODE");
    this->resetTest();

    this->component.configureOutput(GpioDriver::Group::PB, GpioDriver::Pin::PIN_4);

    this->invokeReadAndAssertStatus(Drv::GpioStatus::INVALID_MODE);
}

}  // namespace Samd21

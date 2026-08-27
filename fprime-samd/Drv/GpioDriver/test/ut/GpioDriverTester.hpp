// ======================================================================
// \title  GpioDriverTester.hpp
// \author tumbar
// \brief  hpp file for GpioDriver test harness implementation class
// ======================================================================

#ifndef Samd21_GpioDriverTester_HPP
#define Samd21_GpioDriverTester_HPP

#include "Fw/Types/BasicTypes.hpp"
#include "fprime-samd/Drv/GpioDriver/GpioDriver.hpp"
#include "fprime-samd/Drv/GpioDriver/GpioDriverGTestBase.hpp"
#include "fprime-samd/Drv/GpioDriver/GpioDriverHardware.hpp"

namespace Samd21 {

class GpioDriverTester : public GpioDriverGTestBase {
  public:
    // Maximum size for histories
    static constexpr FwSizeType MAX_HISTORY_SIZE = 10;

    // Test instance ID
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Only one gpioRead / gpioWrite port on the interface
    static constexpr FwIndexType PORT_NUM = 0;

    // Construction and destruction
    GpioDriverTester();
    ~GpioDriverTester();

    // Tests

    //! configure() forwards every argument to the HAL for OUTPUT mode
    void testConfigureOutput();
    //! configure() forwards every argument to the HAL for INPUT mode,
    //! exercising pull-up, pull-down, and the no-pull case
    void testConfigureInput();
    //! configure() reaches the HAL for every group/pin combination
    void testConfigureAllPins();

    //! gpioWrite on a configured output pin drives the HAL and returns OP_OK
    void testWriteNominal();
    //! gpioRead on a configured input pin reads the HAL and returns OP_OK
    void testReadNominal();

    //! gpioWrite before configure() returns NOT_OPENED
    void testWriteUnconfigured();
    //! gpioRead before configure() returns NOT_OPENED
    void testReadUnconfigured();

    //! gpioWrite on an input-configured pin returns INVALID_MODE
    void testWriteWrongMode();
    //! gpioRead on an output-configured pin returns INVALID_MODE
    void testReadWrongMode();

    //! configureInput() forwards each ExternalInterruptMode to the HAL's
    //! configureExternalInterrupt(), and does not call it for NONE
    void testConfigureInputExternalInterrupt();
    //! An edge simulated on a pin configured with an interrupt mode dispatches
    //! through the HAL to gpioInterruptIsr() and emits a cycle on gpioInterrupt
    //! when the port is connected
    void testInterruptFiresWhenConnected();
    //! gpioInterruptIsr() is a no-op (does not touch the port) when
    //! gpioInterrupt is not connected
    void testInterruptIsrNoOpWhenDisconnected();
    //! Simulating an edge on a pin with no registered handler dispatches to nothing
    void testInterruptNoDispatchWithoutRegisteredHandler();

  private:
    //! Component under test
    GpioDriver component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Reset test + stub hardware state between test actions
    void resetTest();

    //! Configure the component as an input and assert the HAL received the arguments
    void configureInputAndAssert(GpioDriver::Group group,
                                 GpioDriver::Pin pin,
                                 GpioDriver::InputPullMode input_pull_mode);

    //! Configure the component as an output and assert the HAL received the arguments
    void configureOutputAndAssert(GpioDriver::Group group, GpioDriver::Pin pin);

    //! Invoke gpioWrite and assert the returned status
    void invokeWriteAndAssertStatus(const Fw::Logic& state, Drv::GpioStatus expected);

    //! Invoke gpioRead and assert the returned status and (on OP_OK) value
    void invokeReadAndAssertStatus(Drv::GpioStatus expected);
};

}  // namespace Samd21

#endif

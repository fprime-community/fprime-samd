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

  private:
    //! Component under test
    GpioDriver component;

    // Auto-generated helper functions
    void connectPorts();
    void initComponents();

    // Helper functions

    //! Reset test + stub hardware state between test actions
    void resetTest();

    //! Configure the component and assert the HAL received the arguments
    void configureAndAssert(GpioDriver::Group group,
                            GpioDriver::Pin pin,
                            GpioDriver::Mode mode,
                            GpioDriver::InputPullMode input_pull_mode);

    //! Invoke gpioWrite and assert the returned status
    void invokeWriteAndAssertStatus(const Fw::Logic& state, Drv::GpioStatus expected);

    //! Invoke gpioRead and assert the returned status and (on OP_OK) value
    void invokeReadAndAssertStatus(Drv::GpioStatus expected);
};

}  // namespace Samd21

#endif

// ======================================================================
// \title  GpioDriver.hpp
// \author tumbar
// \brief  hpp file for GpioDriver component implementation class
// ======================================================================

#ifndef Samd21_GpioDriver_HPP
#define Samd21_GpioDriver_HPP

#include "fprime-samd/Drv/GpioDriver/GpioDriverComponentAc.hpp"

namespace Samd21 {

class GpioDriver final : public GpioDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct GpioDriver object
    GpioDriver(const char* const compName  //!< The component name
    );

    //! Destroy GpioDriver object
    ~GpioDriver();

    enum class Group : U8 {
        PA = 0,
        PB = 1,
    };

    enum class Pin : U8 {
        PIN_0,
        PIN_1,
        PIN_2,
        PIN_3,
        PIN_4,
        PIN_5,
        PIN_6,
        PIN_7,
        PIN_8,
        PIN_9,
        PIN_10,
        PIN_11,
        PIN_12,
        PIN_13,
        PIN_14,
        PIN_15,
        PIN_16,
        PIN_17,
        PIN_18,
        PIN_19,
        PIN_20,
        PIN_21,
        PIN_22,
        PIN_23,
        PIN_24,
        PIN_25,
        PIN_26,
        PIN_27,
        PIN_28,
        PIN_29,
        PIN_30,
        PIN_31,
    };

    enum class Mode : U8 {
        INPUT,
        OUTPUT,
    };

    //! Selects the pull up/down resistor on an input line
    enum class InputPullMode : U8 {
        PULL_DOWN,  //!< Pull down a floating input line
        PULL_UP,    //!< Pull up a floating input line
    };

    //! Configure this component to control a pin in a certain mode
    void configure(Group group, Pin pin, Mode mode, bool enable_pull, InputPullMode input_pull_mode);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for gpioRead
    //!
    //! Port used to read from a GPIO pin
    Drv::GpioStatus gpioRead_handler(FwIndexType portNum,  //!< The port number
                                     Fw::Logic& state) override;

    //! Handler implementation for gpioWrite
    //!
    //! Port used to write to a GPIO pin
    Drv::GpioStatus gpioWrite_handler(FwIndexType portNum,  //!< The port number
                                      const Fw::Logic& state) override;

    bool m_configured;
    Mode m_mode;
    Group m_group;
    Pin m_pin;
};

}  // namespace Samd21

#endif

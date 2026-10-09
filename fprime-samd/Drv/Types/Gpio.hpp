// ======================================================================
// \title  Gpio.hpp
// \author crsmith
// \brief  GPIO types shared by GpioDriver and PinMux
// ======================================================================

#ifndef Samd21_Gpio_HPP
#define Samd21_Gpio_HPP

#include <Fw/FPrimeBasicTypes.hpp>

namespace Samd21 {
namespace Gpio {

//! Selects the pull up/down resistor on a GPIO input line
enum class InputPullMode : U8 {
    NO_PULL,    //!< The pull up/down resistors are not connected
    PULL_DOWN,  //!< Pull down a floating input line
    PULL_UP,    //!< Pull up a floating input line
};

}  // namespace Gpio
}  // namespace Samd21

#endif

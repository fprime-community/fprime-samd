module Samd21 {
  module Gpio {
    @ Selects the pull up/down resistor on a GPIO input line
    enum InputPullMode : U8 {
      NO_PULL    @< The pull up/down resistors are not connected
      PULL_DOWN  @< Pull down a floating input line
      PULL_UP    @< Pull up a floating input line
    }
  }
}

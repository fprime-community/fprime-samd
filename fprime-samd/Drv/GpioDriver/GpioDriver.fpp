module Samd21 {
    @ Driver for the PORT (GPIO) SAMD21 Peripheral
    passive component GpioDriver {

        @ Note! gpioInterrupt is emitted in ISR context!
        import Drv.Gpio

    }
}

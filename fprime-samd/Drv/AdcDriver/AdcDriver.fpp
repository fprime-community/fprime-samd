module Samd21 {
    @ A driver for the SAMD21 Analog to Digital Converter Peripheral
    passive component AdcDriver {

        @ ADC read request ports (start conversions) - size configured in Samd21AdcConfig.fpp
        sync input port readAdc: [Samd21.ADC_CHANNEL_COUNT] AdcRead

        @ ADC result delivery ports (completed values) - matches readAdc array size
        output port adcResult: [Samd21.ADC_CHANNEL_COUNT] AdcResult

        @ Active-cycler tick (delivers completed conversions from the main context)
        sync input port activeIn: Svc.ActiveSched

    }
}

module Samd21 {
    @ Number of ADC read ports (channels) available
    @ Can be set from 1 to 20 (SAMD21 has 20 ADC inputs)
    @ Lower values save RAM: ~2 bytes per port
    @ Set to the number of ADC channels you actually need
    constant ADC_CHANNEL_COUNT = 2
}

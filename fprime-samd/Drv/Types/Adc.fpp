module Samd21 {

  enum AdcStatus : U8 {
    ADC_OK               = 0 @< Transaction okay
    ADC_NOT_CONFIGURED   = 1 @< Driver not initialized
    ADC_INVALID_CHANNEL  = 2 @< Channel not configured for this port
    ADC_BUSY             = 3 @< Conversion already in progress
    ADC_TIMEOUT          = 4 @< Conversion timeout
    ADC_OVERRUN          = 5 @< Result overrun error
  }

  @ Port to request ADC conversion (non-blocking, async)
  port AdcRead() -> AdcStatus

  @ Port to deliver completed ADC conversion result
  port AdcResult(
    value: U32        @< ADC conversion result
    status: AdcStatus @< Conversion status (ADC_OK or ADC_OVERRUN)
  )

}

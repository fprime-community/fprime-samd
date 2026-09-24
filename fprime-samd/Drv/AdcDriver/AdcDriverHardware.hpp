// ======================================================================
// \title  AdcDriverHardware.hpp
// \author crsmith
// \brief  Hardware abstraction layer for ADC driver peripheral operations
//
// All raw ADC register access, clock/NVIC setup live behind this interface
// so the AdcDriver component logic can be exercised natively against a stub.
// Mirrors the I2c/Usart driver HAL split.
//
// ISR Architecture: The ADC driver uses a singleton pattern with a direct
// ISR trampoline (AdcDriverIsr.cpp) rather than callback registration.
// Tests call component.handleInterrupt() directly to simulate ISR firing.
// ======================================================================

#ifndef Samd21_AdcDriverHardware_HPP
#define Samd21_AdcDriverHardware_HPP

#include "Fw/Types/BasicTypes.hpp"
#include "fprime-samd/Drv/AdcDriver/AdcDriver.hpp"

namespace Samd21 {
namespace AdcHardware {

//! Hardware abstraction layer for the SAMD21 ADC peripheral.
struct AdcHal {
    //! Wait for GCLK synchronization with timeout.
    //!
    //! Blocks until GCLK STATUS.SYNCBUSY clears or timeout expires. Asserts on timeout.
    static void waitForGclkSync();

    //! Wait for ADC synchronization with timeout.
    //!
    //! Blocks until ADC STATUS.SYNCBUSY clears or timeout expires. Asserts on timeout.
    static void waitForAdcSync();

    //! Configure and enable the ADC peripheral with calibration and reference setup.
    //!
    //! Performs the full initialization sequence: clock gating, calibration loading,
    //! reference/resolution/averaging setup, and dummy conversion. Returns once the
    //! peripheral is enabled and ready for conversions.
    static void configure(AdcDriver::VoltageReference ref,
                           AdcDriver::Resolution res,
                           AdcDriver::SampleCount samples,
                           U8 samplingTime,
                           AdcDriver::Gain gain);

    //! Configure a specific ADC channel and enable any required internal resources.
    //!
    //! For internal channels (TEMP, BANDGAP), enables the necessary SYSCTRL bits.
    //! External channels (AIN0-AIN19) require pin muxing to be done externally.
    static void configureChannel(AdcDriver::AdcChannel channel);

    //! Select the ADC input channel and gain, wait for synchronization.
    //!
    //! Writes INPUTCTRL with the specified channel and gain setting.
    static void selectChannel(AdcDriver::AdcChannel channel, U8 gain);

    //! Start an ADC conversion (write SWTRIG.START = 1).
    //!
    //! Returns immediately; conversion completes asynchronously and fires an interrupt.
    static void startConversion();

    //! Read the ADC result register (RESULT).
    static U32 readResult();

    //! Check if result is ready (INTFLAG.RESRDY).
    static bool isResultReady();

    //! Check if overrun occurred (INTFLAG.OVERRUN).
    static bool isOverrun();

    //! Clear the overrun flag (write INTFLAG.OVERRUN = 1).
    static void clearOverrun();

    //! Enable ADC interrupts in NVIC with lowest priority.
    static void enableInterrupt();

    //! Enable RESRDY and OVERRUN interrupts in ADC peripheral.
    static void enableAdcInterrupts();
};

//! Test-only hooks into the stub hardware implementation.
//! Compiled only for native/test builds (not the SAMD21 target); lets unit
//! tests observe HAL arguments, inject hardware state, and fire the ISR.
#ifndef __SAMD21__
struct StubState {
    // configure() capture
    bool configured;
    U32 configure_count;
    AdcDriver::VoltageReference ref;
    AdcDriver::Resolution res;
    AdcDriver::SampleCount samples;
    U8 samplingTime;
    AdcDriver::Gain gain;

    // configureChannel() capture
    U32 configureChannel_count;
    AdcDriver::AdcChannel last_configured_channel;

    // selectChannel() capture
    U32 selectChannel_count;
    AdcDriver::AdcChannel selected_channel;
    U8 selected_gain;

    // startConversion() capture
    U32 startConversion_count;

    // Hardware state injection
    U32 result;               //!< Return value for readResult()
    bool result_ready;        //!< Return value for isResultReady()
    bool overrun;             //!< Return value for isOverrun()

    // Sync wait tracking
    U32 waitForGclkSync_calls;
    U32 waitForAdcSync_calls;

    // Flag to track overrun clears
    U32 clearOverrun_count;
};

//! Get the mutable stub state (shared across all HAL calls)
StubState& getStubState();

//! Reset the stub state for a clean test run
void resetStubState();
#endif

}  // namespace AdcHardware
}  // namespace Samd21

#endif

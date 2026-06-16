// ======================================================================
// \title  RtcDriver.hpp
// \author tumbar
// \brief  hpp file for RtcDriver component implementation class
// ======================================================================

#ifndef Samd21_RtcDriver_HPP
#define Samd21_RtcDriver_HPP

#include "fprime-samd/Drv/RtcDriver/RtcDriverComponentAc.hpp"

namespace Samd21 {

class RtcDriver final : public RtcDriverComponentBase {
  public:
    enum class TickRate {
        TICK_1_HZ = 1,
        TICK_2_HZ = 2,
        TICK_4_HZ = 4,
        TICK_8_HZ = 8,
        TICK_16_HZ = 16,
        TICK_32_HZ = 32,
        TICK_64_HZ = 64,
        TICK_128_HZ = 128,
    };

    enum class ClockSource {
        //! Use the standard internal oscillator 32kHz
        //! This oscillator should only be used if not using ULP or
        //! external is not connected as it can contribute high drift
        //! (~32 kHz, ±3%, low power)
        InternalOscillator,

        //! Use the external 32kHz crystal oscillator
        //! This is recommended if high precision is needed
        //! (32.768 kHz, ±20 ppm, high accuracy)
        ExternalOscillator,

        //! Use the internal ultra low power 32kHz clock
        //! This can contribute to drift though it uses the least amount of power
        //! (~32 kHz, ±10%, lowest power)
        UltraLowPowerOscillator,
    };

    /// All internal RTC input clocks unprescaled come in at around this frequency
    constexpr static U32 INTERNAL_CLOCK_FREQ_HZ = 32768;

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Configure the realtime clock tick rate and clock source
    void configure(ClockSource clk_source, TickRate rate);

    //! Enable the RTC interrupt and peripheral
    void enable();

    //! Blocking call that will wait for the next interrupt.
    //!
    //! If the next interrupt comes from the RTC, emit a signal on the cycle output port
    void cycle();

    //! Construct RtcDriver object
    RtcDriver(const char* const compName  //!< The component name
    );

    //! Destroy RtcDriver object
    ~RtcDriver();
};

}  // namespace Samd21

#endif

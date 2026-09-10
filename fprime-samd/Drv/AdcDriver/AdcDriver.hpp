// ======================================================================
// \title  AdcDriver.hpp
// \author crsmith
// \brief  hpp file for AdcDriver component implementation class
// ======================================================================

#ifndef Samd21_AdcDriver_HPP
#define Samd21_AdcDriver_HPP

#include "default/samd-config/FppConstantsAc.hpp"
#include "fprime-samd/Drv/AdcDriver/AdcDriverComponentAc.hpp"

namespace Samd21 {

class AdcDriver final : public AdcDriverComponentBase {
  public:
    //! Voltage reference selection
    enum class VoltageReference : U8 {
        INT1V = 0,    //!< Internal 1.0V reference (most accurate for low voltages)
        INTVCC0 = 1,  //!< Internal VDDANA/1.48 reference (~2.2V if VDDANA=3.3V)
        INTVCC1 = 2,  //!< Internal VDDANA/2 reference (~1.65V if VDDANA=3.3V)
        VREFA = 3,    //!< External reference on VREFA pin
        VREFB = 4     //!< External reference on VREFB pin
    };

    //! ADC resolution selection
    enum class Resolution : U8 {
        RES_8BIT = 0,   //!< 8-bit resolution (0-255)
        RES_10BIT = 1,  //!< 10-bit resolution (0-1023)
        RES_12BIT = 2   //!< 12-bit resolution (0-4095)
    };

    //! Hardware averaging/oversampling sample count
    enum class SampleCount : U8 {
        SAMPLES_1 = 0,     //!< No averaging, single sample (fastest)
        SAMPLES_2 = 1,     //!< Average 2 samples
        SAMPLES_4 = 2,     //!< Average 4 samples
        SAMPLES_8 = 3,     //!< Average 8 samples
        SAMPLES_16 = 4,    //!< Average 16 samples
        SAMPLES_32 = 5,    //!< Average 32 samples
        SAMPLES_64 = 6,    //!< Average 64 samples
        SAMPLES_128 = 7,   //!< Average 128 samples
        SAMPLES_256 = 8,   //!< Average 256 samples
        SAMPLES_512 = 9,   //!< Average 512 samples
        SAMPLES_1024 = 10  //!< Average 1024 samples
    };

    //! ADC gain selection (affects input voltage range)
    enum class Gain : U8 {
        GAIN_1X = 0,     //!< Gain 1x (input range 0 to Vref)
        GAIN_DIV2 = 0xF  //!< Gain 0.5x (divides input by 2, extends range to 2×Vref, register value 0xF)
    };

    //! ADC input channel selection
    enum class AdcChannel : U8 {
        AIN0 = 0,
        AIN1 = 1,
        AIN2 = 2,
        AIN3 = 3,
        AIN4 = 4,
        AIN5 = 5,
        AIN6 = 6,
        AIN7 = 7,
        AIN8 = 8,
        AIN9 = 9,
        AIN10 = 10,
        AIN11 = 11,
        AIN12 = 12,
        AIN13 = 13,
        AIN14 = 14,
        AIN15 = 15,
        AIN16 = 16,
        AIN17 = 17,
        AIN18 = 18,
        AIN19 = 19,
        TEMP = 0x18,        //!< Internal temperature sensor (requires SYSCTRL->VREF.TSEN enabled)
        BANDGAP = 0x19,     //!< Internal bandgap reference (requires SYSCTRL->VREF.BGOUTEN enabled)
        SCALEDIOVCC = 0x1A  //!< Scaled IOVCC (1/4 of IOVCC)
    };

    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct AdcDriver object
    AdcDriver(const char* const compName  //!< The component name
    );

    //! Destroy AdcDriver object
    ~AdcDriver();

    //! Configure the ADC peripheral (voltage reference, resolution, averaging, sampling time, gain)
    //! \param ref Voltage reference selection
    //! \param res ADC resolution (8/10/12 bits)
    //! \param samples Hardware averaging/oversampling count
    //! \param samplingTime SAMPCTRL.SAMPLEN value (0-63), controls sampling duration
    //! \param gain Input gain selection (affects input voltage range relative to Vref)
    void configure(VoltageReference ref, Resolution res, SampleCount samples, U8 samplingTime, Gain gain);

    //! Configure a specific ADC channel for a port
    //! \param portNum Port index (0 to ADC_CHANNEL_COUNT-1)
    //! \param channel ADC input channel (AIN0-AIN19)
    void configureChannel(FwIndexType portNum, AdcChannel channel);

    //! Interrupt handler called from ADC_Handler ISR
    void handleInterrupt();

    //! Singleton instance for ISR access
    static AdcDriver* s_instance;

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for readAdc - starts ADC conversion asynchronously
    //! \param portNum The port number
    //! \return Status code (ADC_OK, ADC_BUSY, ADC_NOT_CONFIGURED, ADC_INVALID_CHANNEL)
    Samd21::AdcStatus readAdc_handler(FwIndexType portNum) override;

    //! Handler implementation for activeIn - checks for completed conversions and delivers results
    //! \param portNum The port number
    //! \param context Context value (unused)
    bool activeIn_handler(FwIndexType portNum, U32 context) override;

    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    bool m_configured;                            //!< ADC peripheral initialized
    U8 m_gain;                                    //!< ADC gain setting
    AdcChannel m_channels[ADC_CHANNEL_COUNT];     //!< Port-to-channel mapping
    bool m_channelConfigured[ADC_CHANNEL_COUNT];  //!< Track which ports configured

    //! Async conversion state machine
    //!
    //! Deliberately lock-free: readAdc_handler/activeIn_handler (main context) and
    //! handleInterrupt (ISR context) share m_state/m_lastResult/m_pendingPortNum
    //! without a CriticalSection, unlike I2cDriver/UsartDriver. This is safe only
    //! because each transition below has exactly one writer and that writer only
    //! acts on the state left by the *other* side, so the two contexts can never
    //! observe or mutate the same field at the same time:
    //!   IDLE -> CONVERTING            written by readAdc_handler (main)
    //!   CONVERTING -> COMPLETE*       written by handleInterrupt (ISR)
    //!   COMPLETE* -> IDLE             written by activeIn_handler (main)
    //! m_lastResult/m_pendingPortNum follow the same rule: the ISR writes
    //! m_lastResult strictly before publishing COMPLETE*, and activeIn_handler only
    //! reads it after observing COMPLETE*; m_pendingPortNum is written by main
    //! context only and read by main context only. If a new transition or a third
    //! reader/writer (e.g. a timeout/abort path) is ever added, re-verify this
    //! single-writer-per-phase invariant still holds, or add a CriticalSection.
    enum class State : U8 {
        IDLE,              //!< No conversion in progress, ready for new request
        CONVERTING,        //!< Conversion in progress, waiting for ISR
        COMPLETE,          //!< Conversion complete, result ready for activeIn to deliver
        COMPLETE_OVERRUN,  //!< Conversion complete with overrun error
    };

    // Async conversion state (shared between readAdc, activeIn, and ISR)
    volatile State m_state;        //!< Current conversion state
    volatile U32 m_lastResult;     //!< Result from last conversion
    FwIndexType m_pendingPortNum;  //!< Which port requested conversion
};

}  // namespace Samd21

#endif

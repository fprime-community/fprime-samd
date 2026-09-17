// ======================================================================
// \title  AdcDriver.cpp
// \author crsmith
// \brief  cpp file for AdcDriver component implementation class
// ======================================================================

#include "fprime-samd/Drv/AdcDriver/AdcDriver.hpp"
#include "fprime-samd/Drv/AdcDriver/AdcDriverHardware.hpp"

namespace Samd21 {

// Singleton instance for ISR access
AdcDriver* AdcDriver::s_instance = nullptr;

// ----------------------------------------------------------------------
// Component construction and destruction
// ----------------------------------------------------------------------

AdcDriver::AdcDriver(const char* const compName)
    : AdcDriverComponentBase(compName),
      m_configured(false),
      m_gain(0),
      m_state(State::IDLE),
      m_lastResult(0),
      m_pendingPortNum(0) {
    // Initialize channel arrays
    for (FwIndexType i = 0; i < ADC_CHANNEL_COUNT; i++) {
        m_channels[i] = AdcChannel::AIN0;
        m_channelConfigured[i] = false;
    }
    // Note: s_instance is set in configure() to ensure only configured driver handles interrupts
}

AdcDriver::~AdcDriver() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

// ----------------------------------------------------------------------
// Configuration methods
// ----------------------------------------------------------------------
// Convenient conversion time calculator https://blog.thea.codes/getting-the-most-out-of-the-samd21-adc/
void AdcDriver::configure(VoltageReference ref, Resolution res, SampleCount samples, U8 samplingTime, Gain gain) {
    // Enforce singleton: only one AdcDriver instance can be configured at a time
    // to ensure ISR routing works correctly. A reconfigure() method could be added
    // in the future to allow runtime reconfiguration of ADC parameters (resolution,
    // reference voltage, etc.) without violating the singleton constraint.
    FW_ASSERT(s_instance == nullptr);

    // Store gain for use in readAdc
    m_gain = static_cast<U8>(gain);

    // Delegate all hardware configuration to the HAL
    AdcHardware::AdcHal::configure(ref, res, samples, samplingTime, gain);

    // Register singleton for ISR access
    s_instance = this;

    // Enable interrupts via HAL (HAL handles priority calculation)
    AdcHardware::AdcHal::enableInterrupt();
    AdcHardware::AdcHal::enableAdcInterrupts();

    m_configured = true;
}

void AdcDriver::configureChannel(FwIndexType portNum, AdcChannel channel) {
    FW_ASSERT(m_configured);
    FW_ASSERT(portNum < ADC_CHANNEL_COUNT);
    FW_ASSERT(!m_channelConfigured[portNum]);

    // Note: Pin muxing should be configured in topology BEFORE calling this for external channels (AIN0-AIN19)
    // Use Samd21::PinMux::configure(PINMUX_xxx) in instances.fpp
    // Internal channels (TEMP, BANDGAP, SCALEDIOVCC) don't require pin configuration

    // Enable internal resources if needed (only TEMP and BANDGAP require hardware setup)
    if (channel == AdcChannel::TEMP || channel == AdcChannel::BANDGAP) {
        AdcHardware::AdcHal::configureChannel(channel);
    }

    // Store channel mapping in component state
    m_channels[portNum] = channel;
    m_channelConfigured[portNum] = true;
}

// ----------------------------------------------------------------------
// Handler implementations for typed input ports
// ----------------------------------------------------------------------

Samd21::AdcStatus AdcDriver::readAdc_handler(FwIndexType portNum) {
    // Validate configuration
    if (!m_configured) {
        return Samd21::AdcStatus::ADC_NOT_CONFIGURED;
    }
    if (!m_channelConfigured[portNum]) {
        return Samd21::AdcStatus::ADC_INVALID_CHANNEL;
    }

    // Check if ADC is already busy
    if (m_state != State::IDLE) {
        return Samd21::AdcStatus::ADC_BUSY;
    }

    // Select input channel with configured gain via HAL
    AdcHardware::AdcHal::selectChannel(m_channels[portNum], m_gain);

    // Transition to CONVERTING state
    m_state = State::CONVERTING;
    m_pendingPortNum = portNum;

    // Start conversion via HAL (interrupt will fire when complete)
    AdcHardware::AdcHal::startConversion();

    return Samd21::AdcStatus::ADC_OK;
}

bool AdcDriver::activeIn_handler(FwIndexType portNum, U32 context) {
    (void)portNum;
    (void)context;

    // Check if conversion is complete and result ready to deliver
    if (m_state == State::COMPLETE || m_state == State::COMPLETE_OVERRUN) {
        // Determine status based on whether overrun occurred
        Samd21::AdcStatus status =
            (m_state == State::COMPLETE_OVERRUN) ? Samd21::AdcStatus::ADC_OVERRUN : Samd21::AdcStatus::ADC_OK;

        // Deliver result and status to the requesting port via output port
        this->adcResult_out(m_pendingPortNum, m_lastResult, status);

        // Return to IDLE state - ADC is now available for new conversion
        m_state = State::IDLE;
    } else {
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------
// Interrupt handler
// ----------------------------------------------------------------------

void AdcDriver::handleInterrupt() {
    // Only process interrupt if we're actually waiting for a conversion
    if (m_state != State::CONVERTING) {
        // Spurious interrupt - clear flags but don't change state
        if (AdcHardware::AdcHal::isOverrun()) {
            AdcHardware::AdcHal::clearOverrun();
        }
        if (AdcHardware::AdcHal::isResultReady()) {
            (void)AdcHardware::AdcHal::readResult();  // Clear RESRDY by reading
        }
        return;
    }

    bool overrun = false;

    // Check for overrun error first via HAL
    if (AdcHardware::AdcHal::isOverrun()) {
        // Clear overrun flag via HAL
        AdcHardware::AdcHal::clearOverrun();
        overrun = true;
    }

    // Check if RESRDY flag is set via HAL (this happens even if OVERRUN occurred)
    if (AdcHardware::AdcHal::isResultReady()) {
        // Read result via HAL (reading RESULT automatically clears RESRDY flag per SAMD21 datasheet §33.6.5)
        m_lastResult = AdcHardware::AdcHal::readResult();

        // Transition to appropriate COMPLETE state
        m_state = overrun ? State::COMPLETE_OVERRUN : State::COMPLETE;
    }
}

}  // namespace Samd21

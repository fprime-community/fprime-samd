// ======================================================================
// \title  SpiDriver.hpp
// \author tumbar
// \brief  hpp file for SpiDriver component implementation class
// ======================================================================

#ifndef Samd21_SpiDriver_HPP
#define Samd21_SpiDriver_HPP

#include "config/FwIndexTypeAliasAc.h"
#include "fprime-samd/Drv/SpiDriver/SpiDriverComponentAc.hpp"
#include "fprime-samd/Drv/Types/SercomKindEnumAc.hpp"
#include "fprime-samd/Drv/Types/ThinBuffer.hpp"

namespace Samd21 {

class SpiDriver final : public SpiDriverComponentBase {
  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct SpiDriver object
    SpiDriver(const char* const compName  //!< The component name
    );

    //! Destroy SpiDriver object
    ~SpiDriver();

    //! This bit selects the data order when a character is shifted out from the shift register.
    enum class DataOrder {
        MSB = 0,  //!< MSB is transferred first.
        LSB = 1,  //!< LSB is transferred first.
    };

    //! In combination with the Clock Phase bit (CPHA), this bit determines the SPI transfer mode.
    enum class ClockPolarity {
        IdleLow = 0,   //!< SCK is low when idle. The leading edge of a clock cycle is a rising edge, while the trailing
                       //!< edge is a falling edge.
        IdleHigh = 1,  //!< SCK is high when idle. The leading edge of a clock cycle is a falling edge, while the
                       //!< trailing edge is a rising edge.
    };

    //! In combination with the Clock Polarity bit (CPOL), this bit determines the SPI transfer mode.
    enum class ClockPhase {
        SampleOnLeadingSck = 0,  //!< The data is sampled on a leading SCK edge and changed on a trailing SCK edge.
        SampleOnRisingSck = 1,   //!< The data is sampled on a trailing SCK edge and changed on a leading SCK edge.
    };

    //! These bits define the data in (DI/MISO) pad configurations.
    enum class DataInPinout {
        PAD0 = 0x0,  //!< SERCOM PAD[0] is used as data input
        PAD1 = 0x1,  //!< SERCOM PAD[1] is used as data input
        PAD2 = 0x2,  //!< SERCOM PAD[2] is used as data input
        PAD3 = 0x3,  //!< SERCOM PAD[3] is used as data input
    };

    //! This bit defines the available pad configurations for data out (MOSI), the serial clock (SCK) and the SPI
    //! select (SS). In Client operation, the SPI Select line (SS) is controlled by DOPO. In host operation, the
    //! SPI Select line (SS) is either controlled by DOPO when CTRLB.MSSEN = 1, or by a GPIO driven by the
    //! application when CTRLB.MSSEN = 0.
    enum class DataOutPinout {
        MOSI_0_SCK_1_CS_2 = 0x0,  //!< MOSI on PAD[0], SCK on PAD[1], Chip select on PAD[2] (when MSSEN = 1)
        MOSI_2_SCK_3_CS_1 = 0x1,  //!< MOSI on PAD[2], SCK on PAD[3], Chip select on PAD[1] (when MSSEN = 1)
        MOSI_3_SCK_1_CS_2 = 0x2,  //!< MOSI on PAD[3], SCK on PAD[1], Chip select on PAD[2] (when MSSEN = 1)
        MOSI_0_SCK_3_CS_1 = 0x3,  //!< MOSI on PAD[0], SCK on PAD[3], Chip select on PAD[1] (when MSSEN = 1)
    };

    //! This bit defines the functionality in standby sleep mode.
    enum class RunInStandby {
        DISABLED,  //!< GCLK_SERCOMx_CORE is disabled and the I2C host will not operate in standby sleep mode.
        ENABLED,   //!< GCLK_SERCOMx_CORE is enabled in all sleep modes.
    };

    //! This bit enables hardware SPI Select (SS/Chip select) control.
    //! If this enabled, the PAD selected by [DataOutPinout] will be controlled by the SERCOM.
    enum class HardwareChipSelect {
        DISABLED = 0,  //!< Hardware SS control is disabled.
        ENABLED = 1,   //!< Hardware SS control is enabled.
    };

    enum class CharacterSize {
        BITS_8 = 0x0,  //!< 8 bits
        BITS_9 = 0x1,  //!< 9 bits
    };

    //! Configure a SERCOM device for SPI Master (host) mode.
    void configure(SercomKind sercom,
                   U32 baud_rate_khz,
                   DataOrder data_order,
                   ClockPolarity clock_polarity,
                   ClockPhase clock_phase,
                   DataInPinout data_in_pinout,
                   DataOutPinout data_out_pinout,
                   RunInStandby run_in_standby,
                   HardwareChipSelect hardware_chipselect);

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for SpiWriteRead
    void SpiWriteRead_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& writeBuffer,
                              Fw::Buffer& readBuffer) override;

    //! Handler implementation for dmaReplyIn
    //!
    //! A signal from the DMAC that a request has finished.
    //! This signal comes inside an ISR!
    void dmaReplyIn_handler(FwIndexType portNum,  //!< The port number
                            const Samd21::Dma::Reply& reply) override;

    SercomKind m_sercom;

    //! Currently executing port number
    FwIndexType m_portNum;

    //! Buffers held to track the active job
    Samd21::ThinBuffer m_read;
    Samd21::ThinBuffer m_write;

    //! Flag tracking if this driver has been configured
    bool m_configured;

    //! Status flag that tracks if there is an active SPI transaction
    bool m_busy;

    //! Flag indicating if we got the Rx buffer back from the DMA
    bool m_rx_busy;

    //! Flag indicating if we got the Tx buffer back from the DMA
    bool m_tx_busy;

    bool m_hardware_chip_select;
};

}  // namespace Samd21

#endif

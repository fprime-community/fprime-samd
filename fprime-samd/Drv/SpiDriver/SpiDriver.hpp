// ======================================================================
// \title  SpiDriver.hpp
// \author tumbar
// \brief  hpp file for SpiDriver component implementation class
// ======================================================================

#ifndef Samd21_SpiDriver_HPP
#define Samd21_SpiDriver_HPP

#include "Fw/Comp/PassiveComponentBase.hpp"
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
        SampleOnLeadingSck = 0,   //!< The data is sampled on a leading SCK edge and changed on a trailing SCK edge.
        SampleOnTrailingSck = 1,  //!< The data is sampled on a trailing SCK edge and changed on a leading SCK edge.
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
        DISABLED,  //!< GCLK_SERCOMx_CORE is disabled and the SPI host will not operate in standby sleep mode.
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

    //! Compute the BAUD register value for SPI host mode.
    //!
    //! In SPI host operation the baud-rate generator runs in Synchronous mode with
    //! the 8-bit BAUD register (§27.6.2.3). From the Synchronous row of the Baud
    //! Rate Equations table (§25.6.2.3):
    //!
    //!     fBAUD = fref / (2*(BAUD + 1))   =>   BAUD = fref/(2*fBAUD) - 1
    //!
    //! where fref is the GCLK_SERCOMx_CORE frequency (f_ref_hz). The result is
    //! rounded so the real SCK never exceeds the requested rate. Asserts when the
    //! rate is zero, above fref/2, or too slow for the 8-bit BAUD register.
    static U8 calculateBaud(U32 f_ref_hz, U32 baud_rate_khz);

    //! Change the SCK rate of a configured driver (e.g. SD card: slow for identification,
    //! fast afterwards). Returns SPI_CONFIG_ERR before configure() and SPI_BUSY_ERR while a
    //! transfer is in flight, leaving the rate unchanged; otherwise applies the rate and
    //! returns SPI_OK. Asserts only for a rate the clock cannot produce (calculateBaud).
    //! Implementation of the setBaudRate port; callable directly from C++ as well.
    Drv::SpiStatus setBaudRate(U32 baud_rate_khz);

    //! Configure a SERCOM device for SPI Master (host) mode. Registers the SERCOM ERROR
    //! interrupt, deasserts every connected software chip select and marks the driver
    //! configured. Asserts if called twice, or if a connected chip-select GPIO cannot be
    //! driven (not opened as an output before this call).
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
    //! A latched reply: the caller's buffers and the status to deliver on activeIn.
    struct PendingReply {
        Samd21::ThinBuffer write;
        Samd21::ThinBuffer read;
        Drv::SpiStatus::T status;
        bool pending;
    };

    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for SpiWriteRead (Drv.AsyncGuardedSpi). Starts the transfer
    //! or refuses it (SPI_BUSY_ERR, SPI_CONFIG_ERR, SPI_OTHER_ERR when the software chip
    //! select cannot be asserted); every outcome is latched and delivered by activeIn,
    //! never from inside this call. A request on a slot whose transfer is in
    //! flight, or whose last outcome activeIn has not delivered yet, is latched in the slot's
    //! refusal record (SPI_BUSY_ERR) so it cannot collide with the ISR's outcome record. A
    //! third request on that slot before activeIn runs is a client protocol error and asserts
    //! in the caller's context.
    void SpiWriteRead_handler(FwIndexType portNum,  //!< The port number
                              Fw::Buffer& writeBuffer,
                              Fw::Buffer& readBuffer) override;

    //! Handler implementation for setBaudRate (main context): see setBaudRate()
    Drv::SpiStatus setBaudRate_handler(FwIndexType portNum,  //!< The port number
                                       U32 baud_rate_khz     //!< SCK rate in kHz
                                       ) override;

    //! Handler implementation for activeIn: delivers every latched reply (main context),
    //! per slot the refusal before the outcome. Returns false (no further work this cycle).
    bool activeIn_handler(FwIndexType portNum,  //!< The port number
                          U32 context           //!< The call order
                          ) override;

    //! Handler implementation for dmaReplyIn
    //!
    //! A signal from the DMAC that a request has finished. This signal comes inside an
    //! ISR: it releases the chip select and latches the reply, nothing is delivered. A
    //! BUS_ERROR on either stream aborts the other stream, drains and acknowledges the
    //! SERCOM receiver, releases the bus and latches SPI_OTHER_ERR. A reply that matches no
    //! transfer in flight is ignored.
    void dmaReplyIn_handler(FwIndexType portNum,  //!< The port number
                            const Samd21::Dma::Reply& reply) override;

    // ----------------------------------------------------------------------
    // SERCOM interrupt handling
    // ----------------------------------------------------------------------

    //! Trampoline registered with the SERCOM ISR table
    static void s_spiDriverIsrHandler(Fw::PassiveComponentBase* spiDriverRaw, SercomKind sercom);

    //! SERCOM ERROR interrupt: a receive overrun lost a character, so the MISO DMA
    //! block can never complete. Abort the outstanding DMA streams, release the
    //! chip select and fail the transaction back to the caller. ISR context.
    void isrHandler();

    //! Release the chip select of the transfer in flight (software chip select only)
    void deassertChipSelect();

    //! Copy-and-clear `record` under a critical section and, if it was pending, emit it on
    //! SpiWriteReadComplete[slot] with interrupts enabled.
    void deliver(PendingReply& record, FwIndexType slot);

    //! Latch an outcome for `portNum` to be delivered by the next activeIn. The slot's
    //! outcome record is free by construction (SpiWriteRead_handler sends a request on an
    //! occupied slot to the refusal record instead); the assert guards that invariant, not
    //! a hardware condition.
    void latchReply(FwIndexType portNum,
                    const Fw::Buffer& writeBuffer,
                    const Fw::Buffer& readBuffer,
                    Drv::SpiStatus::T status);

    SercomKind m_sercom;

    //! Currently executing port number
    FwIndexType m_portNum;

    //! Buffers held to track the active job
    Samd21::ThinBuffer m_read;
    Samd21::ThinBuffer m_write;

    //! Flag tracking if this driver has been configured
    bool m_configured;

    enum BusyBit {
        RX_BUSY = (1 << 0),
        TX_BUSY = (1 << 1),
    };

    //! Status flag that tracks while SPI transactions are still active
    U8 m_busy;

    bool m_hardware_chip_select;

    //! One transfer outcome waiting for activeIn, per slot. Written by the ISR paths
    //! (transfer done, DMA bus error, receive overrun) and by requests refused while the
    //! bus is idle; read and cleared by activeIn under a critical section. Invariant: a slot
    //! holds at most one undelivered outcome, counting the transfer in flight.
    PendingReply m_pending[NUM_SPIWRITEREAD_INPUT_PORTS];

    //! One SPI_BUSY_ERR refusal waiting for activeIn, per slot, for a request that arrived
    //! while the slot's outcome record was still owned by the transfer in flight or by an
    //! undelivered completion. Written in the caller's context only.
    PendingReply m_refused[NUM_SPIWRITEREAD_INPUT_PORTS];
};

}  // namespace Samd21

#endif

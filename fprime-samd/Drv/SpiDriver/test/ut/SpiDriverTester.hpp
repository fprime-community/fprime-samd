// ======================================================================
// \title  SpiDriverTester.hpp
// \brief  hpp file for SpiDriver component test harness implementation class
// ======================================================================

#ifndef Samd21_SpiDriverTester_HPP
#define Samd21_SpiDriverTester_HPP

#include "fprime-samd/Drv/SpiDriver/SpiDriver.hpp"
#include "fprime-samd/Drv/SpiDriver/SpiDriverGTestBase.hpp"
#include "fprime-samd/Drv/SpiDriver/SpiDriverHardware.hpp"
#include "fprime-samd/Drv/SpiDriver/SpiDriver_DmaChannelEnumAc.hpp"
#include "samd-config/FppConstantsAc.hpp"

namespace Samd21 {

class SpiDriverTester final : public SpiDriverGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 64;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

    // Largest transaction exercised by the tests
    static const FwSizeType MAX_TEST_SIZE = 64;

    // SERCOM core clock used for the baud-rate tests (48 MHz GCLK0)
    static const U32 TEST_F_REF_HZ = 48000000;

    // Fake SERCOM DATA register address returned by the stub HAL
    static const U32 TEST_DATA_REGISTER = 0x42000C28;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object SpiDriverTester
    SpiDriverTester();

    //! Destroy object SpiDriverTester
    ~SpiDriverTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! configure() programs the HAL and deasserts every software chip select
    void testConfigureSoftwareChipSelect();

    //! configure() with hardware chip select never drives the GPIO ports
    void testConfigureHardwareChipSelect();

    //! A second configure() asserts
    void testConfigureTwice();

    //! A request before configure() asserts
    void testRequestBeforeConfigure();

    //! Nominal full-duplex transaction on every port, both DMA completion orders
    void testTransaction();

    //! A request while a transaction is in flight is rejected with SPI_OTHER_ERR
    void testBusy();

    //! Hardware chip select transactions never touch the GPIO ports
    void testHardwareChipSelectTransaction();

    //! Mismatched write/read sizes assert
    void testSizeMismatch();

    //! A DMA reply with no transaction in flight asserts
    void testUnexpectedDmaReply();

    //! calculateBaud: exact values, round-up, and bounds
    void testCalculateBaud();

    //! Randomized ports, sizes and DMA completion orders
    void testRandomTransactions();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports (capture the port index, which the
    // generated history does not record)
    // ----------------------------------------------------------------------

    void from_SpiReply_handler(FwIndexType portNum,
                               Fw::Buffer& writeBuffer,
                               Fw::Buffer& readBuffer,
                               const Drv::SpiStatus& status) override;

    Drv::GpioStatus from_chipSelectGpioOut_handler(FwIndexType portNum, const Fw::Logic& state) override;

    void from_dmaTransactionOut_handler(FwIndexType portNum,
                                        const Samd21::Dma::TriggerSource& trigger,
                                        const Samd21::Dma::TransactionType& action,
                                        const Samd21::Dma::Priority& priority,
                                        U32 sourceAddr,
                                        U32 destAddr,
                                        U16 beat_count,
                                        const Samd21::Dma::BeatSize& beatSize,
                                        bool incrementSource,
                                        bool incrementDestination,
                                        const Samd21::Dma::AddressIncrementStepSize& stepSize,
                                        const Samd21::Dma::StepSelection& stepSelection) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Clear generated histories, captured port indices, and the HAL stub
    void clearAll();

    //! Configure the component with the test settings and clear histories afterwards
    void configure(SpiDriver::HardwareChipSelect hardwareChipSelect);

    //! Issue a request on port and check chip select + the two DMA jobs were queued
    void startTransaction(FwIndexType port, FwSizeType size);

    //! Deliver one DMA completion
    void completeDma(SpiDriver_DmaChannel::T channel);

    //! Deliver both DMA completions (misoFirst picks the order) and check the reply
    void finishTransaction(FwIndexType port, bool misoFirst);

    //! Assert the n-th chip select call was (port, state)
    void assertChipSelect(FwSizeType index, FwIndexType port, Fw::Logic::T state);

    //! Assert exactly one reply was emitted on port with the request buffers and status
    void assertReply(FwIndexType port, Drv::SpiStatus::T status);

    //! Address of a buffer as the DMA port carries it
    static U32 address(const U8* data);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    SpiDriver component;

    //! Request buffers (one pair per port so busy tests can overlap requests)
    U8 m_writeData[Samd21::SpiPorts][MAX_TEST_SIZE];
    U8 m_readData[Samd21::SpiPorts][MAX_TEST_SIZE];
    Fw::Buffer m_writeBuffer[Samd21::SpiPorts];
    Fw::Buffer m_readBuffer[Samd21::SpiPorts];

    //! Port indices seen on the from ports, parallel to the generated histories
    FwIndexType m_replyPorts[MAX_HISTORY_SIZE];
    FwIndexType m_chipSelectPorts[MAX_HISTORY_SIZE];
    FwIndexType m_dmaPorts[MAX_HISTORY_SIZE];

    //! Whether the component was configured with hardware chip select
    bool m_hardwareChipSelect;
};

}  // namespace Samd21

#endif

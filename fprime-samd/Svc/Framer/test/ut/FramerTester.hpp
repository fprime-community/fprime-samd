// ======================================================================
// \title  FramerTester.hpp
// \author tumbar
// \brief  hpp file for Framer component test harness implementation class
// ======================================================================

#ifndef Samd21_FramerTester_HPP
#define Samd21_FramerTester_HPP

#include "fprime-samd/Svc/Framer/Framer.hpp"
#include "fprime-samd/Svc/Framer/FramerGTestBase.hpp"

namespace Samd21 {

class FramerTester final : public FramerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 10;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object FramerTester
    FramerTester();

    //! Destroy object FramerTester
    ~FramerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test basic single packet framing
    void testSinglePacket();

    //! Test multi-packet accumulation
    void testMultiPacketAccumulation();

    //! Test buffer overflow and automatic flush
    void testBufferOverflow();

    //! Test double-buffer lifecycle with send/return
    void testDoubleBufferLifecycle();

    //! Test backpressure and packet drops
    void testBackpressure();

    //! Test schedIn periodic flush
    void testSchedInFlush();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Send a ComBuffer through the framer
    void sendComBuffer(const U8* data, FwSizeType size);

    //! Verify that a framed buffer was sent to the driver
    void verifyFramedBuffer(FwSizeType expectedPayloadSize);

    //! Return a buffer to the framer (simulating driver completion)
    void returnBuffer(FwIndexType index);

    //! Create a test ComBuffer with specified size
    Fw::ComBuffer makeComBuffer(FwSizeType size, U8 fillValue = 0xAA);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    Framer component;

    //! Sent buffers tracking
    struct SentBuffer {
        U8 data[256];
        FwSizeType size;
        bool valid;
    };
    SentBuffer m_sentBuffers[2];
    FwIndexType m_numSentBuffers;
};

}  // namespace Samd21

#endif

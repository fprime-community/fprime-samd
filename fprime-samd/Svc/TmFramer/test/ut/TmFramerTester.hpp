// ======================================================================
// \title  TmFramerTester.hpp
// \brief  hpp file for TmFramer component test harness implementation class
// ======================================================================

#ifndef Samd21_TmFramerTester_HPP
#define Samd21_TmFramerTester_HPP

#include "fprime-samd/Svc/TmFramer/TmFramer.hpp"
#include "fprime-samd/Svc/TmFramer/TmFramerGTestBase.hpp"

namespace Samd21 {

class TmFramerTester final : public TmFramerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static const FwSizeType MAX_HISTORY_SIZE = 20;

    // Instance ID supplied to the component instance under test
    static const FwEnumStoreType TEST_INSTANCE_ID = 0;

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    //! Construct object TmFramerTester
    TmFramerTester();

    //! Destroy object TmFramerTester
    ~TmFramerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    //! Test that a single packet is only sent on flush, and framed correctly
    void testNominalFraming();

    //! Test that multiple packets accumulate into one frame before flush
    void testMultiPacketAccumulation();

    //! Test that a packet which would leave an unfillable idle-pad remainder
    //! triggers an early flush instead of overflowing
    void testDeadZoneAvoidance();

    //! Test that a packet which would overflow the data field triggers a flush
    void testBufferOverflow();

    //! Test double-buffer send/return lifecycle across multiple frames
    void testDoubleBufferLifecycle();

    //! Test packet drops when both buffers are in flight (backpressure)
    void testBackpressure();

    //! Test periodic flush triggered by schedIn
    void testSchedInFlush();

    //! Test that TELEM and LOG packets get independent, correctly-wrapping
    //! per-APID sequence counts
    void testPerApidSequenceCounts();

    //! Test that Master/Virtual Frame Count wrap mod-256
    void testFrameCountWrapAround();

    //! Test that returning a buffer the framer didn't send asserts
    void testUnexpectedBufferReturn();

    //! Test that a distinct APID beyond Samd21::FramerConfig::MAX_TRACKED_APIDS asserts
    //! rather than silently sharing another APID's sequence count
    void testApidTrackingOverflow();

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    //! Connect ports
    void connectPorts();

    //! Initialize components
    void initComponents();

    //! Build a ComBuffer with a leading APID descriptor (as Fw::ComPacket::serializeBase()
    //! would write) followed by `size` payload bytes of `fillValue`.
    Fw::ComBuffer makeComBuffer(ComCfg::Apid apid, FwSizeType size, U8 fillValue = 0xAA);

    //! Send a packet through comPacketQueueIn
    void sendPacket(ComCfg::Apid apid, FwSizeType size, U8 fillValue = 0xAA);

    //! Fill the active buffer's data field to within one Space Packet header's worth of
    //! ComCfg::TmFrameFixedSize's data field capacity, using max-size packets. Used to set up
    //! an overflow on the next sendPacket() call.
    void fillActiveBufferToCapacity();

    //! Return the buffer at the given drvSendOut history index back to the framer,
    //! simulating driver completion
    void returnBuffer(FwIndexType historyIndex);

    U16 getFrameScId(const U8* frameData);    //!< Spacecraft ID from a TM frame - no boundary check
    U8 getFrameVcId(const U8* frameData);     //!< Virtual Channel ID from a TM frame - no boundary check
    U8 getFrameMcCount(const U8* frameData);  //!< Master Frame Count from a TM frame - no boundary check
    U8 getFrameVcCount(const U8* frameData);  //!< Virtual Frame Count from a TM frame - no boundary check

    //! Space Packet APID + sequence count at a given data-field offset - no boundary check
    U16 getSpacePacketApid(const U8* frameData, FwSizeType dataFieldOffset);
    U16 getSpacePacketSeqCount(const U8* frameData, FwSizeType dataFieldOffset);

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    TmFramer component;
};

}  // namespace Samd21

#endif

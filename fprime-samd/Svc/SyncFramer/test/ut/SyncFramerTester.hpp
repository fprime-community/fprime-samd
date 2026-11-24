// ======================================================================
// \title  SyncFramerTester.hpp
// \author tumbar
// \brief  hpp file for SyncFramer component test harness implementation class
// ======================================================================

#ifndef Samd21_SyncFramerTester_HPP
#define Samd21_SyncFramerTester_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "fprime-samd/Svc/SyncFramer/SyncFramer.hpp"
#include "fprime-samd/Svc/SyncFramer/SyncFramerGTestBase.hpp"

namespace Samd21 {

class SyncFramerTester final : public SyncFramerGTestBase {
  public:
    // ----------------------------------------------------------------------
    // Constants
    // ----------------------------------------------------------------------

    // Maximum size of histories storing events, telemetry, and port outputs
    static constexpr FwSizeType MAX_HISTORY_SIZE = 32;

    // Instance ID supplied to the component instance under test
    static constexpr FwEnumStoreType TEST_INSTANCE_ID = 0;

    //! Bytes of framing around each packet
    static constexpr FwSizeType FRAME_OVERHEAD = 12;

    //! Capacity of the byte stream recorded from drvSendOut
    static constexpr FwSizeType STREAM_CAPACITY = 4 * (FW_COM_BUFFER_MAX_SIZE + FRAME_OVERHEAD);

  public:
    // ----------------------------------------------------------------------
    // Construction and destruction
    // ----------------------------------------------------------------------

    SyncFramerTester();
    ~SyncFramerTester();

  public:
    // ----------------------------------------------------------------------
    // Tests
    // ----------------------------------------------------------------------

    void testFrameLayout();
    void testKnownCrc();
    void testGroundDeframes();
    void testPacketSizes();
    void testBackToBackFrames();
    void testDriverErrorIgnored();
    void testDrvConnectedNoOp();

  private:
    // ----------------------------------------------------------------------
    // Handlers for typed from ports
    // ----------------------------------------------------------------------

    //! Record the bytes of each send as the driver would put them on the wire. The
    //! buffers the framer sends are only valid for the duration of the call.
    Drv::ByteStreamStatus from_drvSendOut_handler(FwIndexType portNum, Fw::Buffer& sendBuffer) override;

  private:
    // ----------------------------------------------------------------------
    // Helper functions
    // ----------------------------------------------------------------------

    void connectPorts();
    void initComponents();

    //! Clear the port history and the recorded byte stream
    void resetTest();

    //! Fill a ComBuffer with `size` random bytes
    static void fillRandom(Fw::ComBuffer& packet, FwSizeType size);

    //! Send a packet through comPacketIn
    void sendPacket(Fw::ComBuffer& packet);

    //! Assert the recorded stream at `offset` holds one well-formed frame carrying `packet`:
    //! header (start word, length), payload, then the CRC32 of header + payload
    //! \return Offset just past the frame
    FwSizeType assertFrame(FwSizeType offset, const Fw::ComBuffer& packet);

    //! Read a big-endian U32 from the recorded stream
    U32 streamU32(FwSizeType offset) const;

  private:
    // ----------------------------------------------------------------------
    // Member variables
    // ----------------------------------------------------------------------

    //! The component under test
    SyncFramer component;

    //! Concatenation of every buffer sent on drvSendOut
    U8 m_stream[STREAM_CAPACITY];
    FwSizeType m_streamSize;

    //! Status returned from drvSendOut
    Drv::ByteStreamStatus m_sendStatus;
};

}  // namespace Samd21

#endif

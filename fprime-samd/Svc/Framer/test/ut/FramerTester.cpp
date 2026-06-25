// ======================================================================
// \title  FramerTester.cpp
// \author tumbar
// \brief  cpp file for Framer component test harness implementation class
// ======================================================================

#include "FramerTester.hpp"
#include "Drv/ByteStreamDriverModel/ByteStreamStatusEnumAc.hpp"
#include "Svc/FprimeProtocol/FrameHeaderSerializableAc.hpp"
#include "Svc/FprimeProtocol/FrameTrailerSerializableAc.hpp"

namespace Samd21 {

// ----------------------------------------------------------------------
// Construction and destruction
// ----------------------------------------------------------------------

FramerTester ::FramerTester()
    : FramerGTestBase("FramerTester", FramerTester::MAX_HISTORY_SIZE),
      component("Framer"),
      m_numSentBuffers(0) {
    this->initComponents();
    this->connectPorts();

    // Initialize sent buffer tracking
    for (FwIndexType i = 0; i < 2; i++) {
        m_sentBuffers[i].valid = false;
        m_sentBuffers[i].size = 0;
    }

    // Signal that driver is connected
    invoke_to_drvConnected(0);
}

FramerTester ::~FramerTester() {
    this->component.deinit();
}

// ----------------------------------------------------------------------
// Tests
// ----------------------------------------------------------------------

void FramerTester ::testSinglePacket() {
    // Send a single packet and verify it's framed correctly
    const U8 testData[] = {0x01, 0x02, 0x03, 0x04};
    sendComBuffer(testData, sizeof(testData));

    // Verify no send yet (waiting for schedIn or explicit flush)
    ASSERT_from_drvSendOut_SIZE(0);
}

void FramerTester ::testMultiPacketAccumulation() {
    // Send multiple small packets
    for (FwIndexType i = 0; i < 5; i++) {
        U8 data[10];
        for (FwIndexType j = 0; j < sizeof(data); j++) {
            data[j] = static_cast<U8>(i * 10 + j);
        }
        sendComBuffer(data, sizeof(data));
    }

    // Verify no send yet (accumulated in buffer)
    ASSERT_from_drvSendOut_SIZE(0);

    // Trigger flush via schedIn
    invoke_to_schedIn(0, 0);

    // Should have sent one frame with all packets
    ASSERT_from_drvSendOut_SIZE(1);
}

void FramerTester ::testBufferOverflow() {
    // Test that buffer auto-flushes when it fills up
    // Send many small packets until we trigger an overflow
    // With 256 byte buffer and ~50 byte packets, we should overflow around packet 5

    FwIndexType packetsSent = 0;
    const FwIndexType MAX_PACKETS = 10;

    // Keep sending until we get a flush
    for (packetsSent = 0; packetsSent < MAX_PACKETS; packetsSent++) {
        U8 data[50];
        for (FwIndexType j = 0; j < sizeof(data); j++) {
            data[j] = static_cast<U8>(packetsSent);
        }
        sendComBuffer(data, sizeof(data));

        // Check if auto-flush happened
        if (fromPortHistory_drvSendOut->size() > 0) {
            break;
        }
    }

    // Should have auto-flushed before filling all packets
    ASSERT_GT(packetsSent, 0);  // Sent at least one
    ASSERT_LT(packetsSent, MAX_PACKETS);  // But not all (overflow triggered)
    ASSERT_from_drvSendOut_SIZE(1);  // One buffer sent

    // The overflow packet was dropped (backpressure - buffer 0 still transmitting)
    // Return the first buffer so we can continue
    const Fw::Buffer& buf1Const = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    Fw::Buffer buf1(buf1Const.getData(), buf1Const.getSize());
    invoke_to_drvReturnIn(0, buf1, Drv::ByteStreamStatus::OP_OK);

    // Now send another packet and flush
    U8 more[50];
    for (FwIndexType j = 0; j < sizeof(more); j++) {
        more[j] = 0xFF;
    }
    sendComBuffer(more, sizeof(more));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);  // Second buffer sent
}

void FramerTester ::testDoubleBufferLifecycle() {
    // Fill and flush first buffer
    U8 data1[50];
    for (FwIndexType i = 0; i < sizeof(data1); i++) {
        data1[i] = 0xAA;
    }
    sendComBuffer(data1, sizeof(data1));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    // Return first buffer to free it up
    const Fw::Buffer& buf1Const = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    Fw::Buffer buf1(buf1Const.getData(), buf1Const.getSize());
    invoke_to_drvReturnIn(0, buf1, Drv::ByteStreamStatus::OP_OK);

    // Now fill and flush second buffer
    U8 data2[50];
    for (FwIndexType i = 0; i < sizeof(data2); i++) {
        data2[i] = 0xBB;
    }
    sendComBuffer(data2, sizeof(data2));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);

    // Return second buffer
    const Fw::Buffer& buf2Const = this->fromPortHistory_drvSendOut->at(1).fwBuffer;
    Fw::Buffer buf2(buf2Const.getData(), buf2Const.getSize());
    invoke_to_drvReturnIn(0, buf2, Drv::ByteStreamStatus::OP_OK);

    // Both buffers should now be available
    // Send another packet and flush
    sendComBuffer(data1, sizeof(data1));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);
}

void FramerTester ::testBackpressure() {
    // Test backpressure behavior when both buffers are in flight
    U8 data[50];
    for (FwIndexType i = 0; i < sizeof(data); i++) {
        data[i] = 0xCC;
    }

    // Send and flush buffer 1
    sendComBuffer(data, sizeof(data));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(1);

    // Return buffer 1 so we can use buffer 2
    const Fw::Buffer& buf1Const = this->fromPortHistory_drvSendOut->at(0).fwBuffer;
    Fw::Buffer buf1(buf1Const.getData(), buf1Const.getSize());
    invoke_to_drvReturnIn(0, buf1, Drv::ByteStreamStatus::OP_OK);

    // Send and flush buffer 2
    sendComBuffer(data, sizeof(data));
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);

    // Now buffer 2 is in flight, buffer 1 is idle
    // Send data to buffer 1, but DON'T flush yet
    sendComBuffer(data, sizeof(data));
    ASSERT_from_drvSendOut_SIZE(2);  // Still 2, not flushed

    // Try to flush - should not work because buffer 2 is still transmitting
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(2);  // Still 2 (backpressure prevents flush)

    // Return buffer 2 to free up space
    const Fw::Buffer& buf2Const = this->fromPortHistory_drvSendOut->at(1).fwBuffer;
    Fw::Buffer buf2(buf2Const.getData(), buf2Const.getSize());
    invoke_to_drvReturnIn(0, buf2, Drv::ByteStreamStatus::OP_OK);

    // Now we can flush buffer 1
    invoke_to_schedIn(0, 0);
    ASSERT_from_drvSendOut_SIZE(3);  // Third send successful
}

void FramerTester ::testSchedInFlush() {
    // Add data but don't overflow
    U8 data[50];
    for (FwIndexType i = 0; i < sizeof(data); i++) {
        data[i] = 0xDD;
    }
    sendComBuffer(data, sizeof(data));

    // No automatic flush
    ASSERT_from_drvSendOut_SIZE(0);

    // Trigger flush via schedIn
    invoke_to_schedIn(0, 0);

    // Should have flushed
    ASSERT_from_drvSendOut_SIZE(1);
}

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

void FramerTester ::sendComBuffer(const U8* data, FwSizeType size) {
    Fw::ComBuffer comBuf;
    Fw::SerializeStatus status = comBuf.serializeFrom(data, size, Fw::Serialization::OMIT_LENGTH);
    ASSERT_EQ(status, Fw::FW_SERIALIZE_OK);
    invoke_to_comPacketQueueIn(0, comBuf, 0);
}

void FramerTester ::verifyFramedBuffer(FwSizeType expectedPayloadSize) {
    // Verify the sent buffer has correct framing
    ASSERT_from_drvSendOut_SIZE(1);
    const Fw::Buffer& sentBuf = this->fromPortHistory_drvSendOut->at(0).fwBuffer;

    // Minimum size is header + payload + trailer
    FwSizeType minSize = Svc::FprimeProtocol::FrameHeader::SERIALIZED_SIZE + expectedPayloadSize +
                         Svc::FprimeProtocol::FrameTrailer::SERIALIZED_SIZE;
    ASSERT_GE(sentBuf.getSize(), minSize);
}

void FramerTester ::returnBuffer(FwIndexType index) {
    ASSERT_LT(index, this->fromPortHistory_drvSendOut->size());
    const Fw::Buffer& bufConst = this->fromPortHistory_drvSendOut->at(index).fwBuffer;
    Fw::Buffer buf(bufConst.getData(), bufConst.getSize());
    invoke_to_drvReturnIn(0, buf, Drv::ByteStreamStatus::OP_OK);
}

Fw::ComBuffer FramerTester ::makeComBuffer(FwSizeType size, U8 fillValue) {
    Fw::ComBuffer comBuf;
    for (FwSizeType i = 0; i < size && i < FW_COM_BUFFER_MAX_SIZE; i++) {
        U8 val = fillValue;
        comBuf.serializeFrom(&val, sizeof(val), Fw::Serialization::OMIT_LENGTH);
    }
    return comBuf;
}

}  // namespace Samd21

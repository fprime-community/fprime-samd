// ======================================================================
// \title  TmFramer.hpp
// \brief  hpp file for CCSDS TM Transfer Frame downlink framer implementation class
// ======================================================================

#ifndef Samd21_TmFramer_HPP
#define Samd21_TmFramer_HPP

#include "Fw/Com/ComBuffer.hpp"
#include "Svc/Ccsds/Types/FppConstantsAc.hpp"
#include "Svc/Ccsds/Types/SpacePacketHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/TMHeaderSerializableAc.hpp"
#include "Svc/Ccsds/Types/TMTrailerSerializableAc.hpp"
#include "config/ApidEnumAc.hpp"
#include "config/FppConstantsAc.hpp"
#include "fprime-samd/Svc/TmFramer/TmFramerComponentAc.hpp"

namespace Samd21 {

class TmFramer final : public TmFramerComponentBase {
    friend class TmFramerTester;

  public:
    // ----------------------------------------------------------------------
    // Component construction and destruction
    // ----------------------------------------------------------------------

    //! Construct TmFramer object
    TmFramer(const char* const compName  //!< The component name
    );

    //! Destroy TmFramer object
    ~TmFramer();

  private:
    // ----------------------------------------------------------------------
    // Handler implementations for typed input ports
    // ----------------------------------------------------------------------

    //! Handler implementation for comPacketQueueIn
    //!
    //! Input port that handles downlink packets. Wraps each ComBuffer in a CCSDS Space
    //! Packet and accumulates it into the active TM Transfer Frame buffer.
    void comPacketQueueIn_handler(FwIndexType portNum,  //!< The port number
                                  Fw::ComBuffer& data,  //!< Buffer containing packet data
                                  U32 context           //!< Call context value; meaning chosen by user
                                  ) override;

    //! Handler implementation for drvConnected
    //!
    //! Signal from the driver that it is ready
    void drvConnected_handler(FwIndexType portNum  //!< The port number
                              ) override;

    //! Handler implementation for drvReturnIn
    //!
    //! Receive buffer back from driver
    void drvReturnIn_handler(FwIndexType portNum,  //!< The port number
                             Fw::Buffer& fwBuffer,
                             const Drv::ByteStreamStatus& status) override;

    //! Handler implementation for schedIn
    //!
    //! Input port that flushes the active Tx buffer to the driver
    void schedIn_handler(FwIndexType portNum,  //!< The port number
                         U32 context           //!< The call order
                         ) override;

    //! Flush the active buffer (closing out the TM frame: idle-pad, CRC, send) to the driver
    void flushActiveBuffer();

    //! Append one CCSDS Space Packet (header + ComBuffer payload) to the active buffer.
    //! Returns the number of bytes written.
    FwSizeType appendSpacePacket(U8* dest, FwSizeType destCapacity, Fw::ComBuffer& data);

    //! Peek the leading FwPacketDescriptorType already embedded in a ComBuffer (written by
    //! Fw::ComPacket::serializeBase() -- see Fw::TlmPacket/Fw::LogPacket) and map it to the
    //! CCSDS APID this framer should assign, WITHOUT mutating `data`'s own read/serialize
    //! cursor (it is still copied byte-for-byte into the Space Packet afterward).
    ComCfg::Apid apidForComBuffer(const Fw::ComBuffer& data) const;

    //! Look up (and advance) the sequence count for a given APID. Only two APIDs are routed
    //! through this framer today (FW_PACKET_TELEM, FW_PACKET_LOG per comPacketQueueIn's two
    //! upstream sources), so a small fixed table is used instead of a general-purpose map
    //! (e.g. Svc::Ccsds::ApidManager's Fw::ArrayMap). Revisit if more downlink APIDs are added.
    U16 nextApidSequenceCount(ComCfg::Apid apid);

    //! Pad the remainder of a not-yet-full TM frame data field with a CCSDS idle Space Packet,
    //! then compute and write the TMTrailer FECF (CRC-16/CCITT-FALSE). `dataFieldUsed` is the
    //! number of data-field bytes already written (after the TMHeader).
    void closeFrame(U8* frameData, FwSizeType dataFieldUsed);

    //! Double-buffer state
    enum BufferState : U8 {
        IDLE,         //!< Buffer is idle and available for accumulation
        ACTIVE,       //!< Buffer is being filled with Space Packets
        TRANSMITTING  //!< Buffer has been sent to driver
    };

    struct TxBuffer {
        U8 data[ComCfg::TmFrameFixedSize];  //!< Buffer storage (one full TM Transfer Frame)
        FwSizeType dataFieldSize;           //!< Bytes written into the data field so far (after TMHeader)
        BufferState state;                  //!< Current state of buffer
    };

    //! Idle-fill pattern used by Svc::Ccsds::TmFramer for idle Space Packet payload bytes;
    //! matched here for consistency with stock fprime CCSDS idle packets.
    static constexpr U8 IDLE_DATA_PATTERN = 0x44;

    //! Fixed set of APIDs this framer tracks a sequence count for. Extend if more downlink
    //! packet sources are ever wired into comPacketQueueIn.
    enum ApidSequenceSlot { TELEM_SLOT, LOG_SLOT, NUM_APID_SEQUENCE_SLOTS };

    bool m_driverConnected;
    TxBuffer m_buffers[2];          //!< Double buffer
    FwIndexType m_activeBufferIdx;  //!< Index of currently active buffer
    U32 m_droppedPackets;           //!< Telemetry tracking number of dropped packets
    U16 m_apidSequenceCounts[NUM_APID_SEQUENCE_SLOTS];  //!< Per-APID sequence count (mod-16384)
    U8 m_masterFrameCount;                              //!< Master Channel Frame Count (mod-256)
    U8 m_virtualFrameCount;                             //!< Virtual Channel Frame Count (mod-256)
};

}  // namespace Samd21

#endif

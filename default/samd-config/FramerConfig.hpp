/*
 * FramerConfig.hpp:
 *
 * Configuration settings for the SAMD21 Framer component.
 */

#ifndef SAMD21_FRAMER_CFG_HPP_
#define SAMD21_FRAMER_CFG_HPP_

namespace Samd21 {
enum FramerConfig {
    //! Maximum size for a single transmit buffer (bytes)
    //! Buffer contains: frame header + multiple accumulated ComBuffers + frame trailer
    //! Larger buffers allow more packet accumulation before flush
    FRAMER_TX_BUFFER_SIZE = 512,

    //! Number of distinct CCSDS APIDs Samd21::TmFramer tracks an independent Space Packet
    //! sequence count for (CCSDS 133.0-B-2 4.1.3.4 requires the count to be per-APID).
    //! Slots are claimed lazily, in order of first sight, and cost
    //! sizeof(TmFramer::ApidSequenceSlot) each.
    //!
    //! 4 covers the APIDs an F Prime downlink actually emits -- FW_PACKET_TELEM and
    //! FW_PACKET_LOG are the two always present, with headroom for FW_PACKET_FILE and one
    //! more -- without a config override. The idle Space Packet TmFramer::closeFrame()
    //! pads with does NOT consume a slot: it is emitted with a fixed sequence control
    //! field and never goes through nextApidSequenceCount().
    //!
    //! Overflow is deliberately non-fatal. A distinct APID arriving once all slots are
    //! claimed is sent with sequence count 0 and bumps the ApidOverflowCount channel,
    //! because that path is reachable while framing a FATAL packet and asserting there
    //! would stop the FATAL from ever reaching the ground. So raising this is a
    //! ground-visible telemetry fix, not a crash fix. Must stay <= 255.
    MAX_TRACKED_APIDS = 4,
};
}  // namespace Samd21

#endif

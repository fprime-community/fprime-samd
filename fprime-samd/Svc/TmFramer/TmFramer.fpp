module Samd21 {
    @ CCSDS TM Transfer Frame downlink framer with static memory (no heap, DMA double-buffered)
    @
    @ Uses schedIn (not activeIn) intentionally: this component only needs periodic flush
    @ cadence for its double-buffer, not ISR-signal-queue draining. The DMA-adjacent
    @ latency-critical work is already owned by the byte-stream driver's own activeIn
    @ connection (see Samd21.UsartDriver); TmFramer just hands completed frames to it.
    passive component TmFramer {

        @ Input port that flushes the active Tx buffer to the driver
        sync input port schedIn: Svc.Sched

        @ Input port that handles downlink packets
        sync input port comPacketQueueIn: Fw.Com

        @ Signal from the driver that it is ready
        sync input port drvConnected: Drv.ByteStreamReady

        @ Send a framed packet to the driver
        output port drvSendOut: Fw.BufferSend

        @ Receive buffer back from driver
        sync input port drvReturnIn: Drv.ByteStreamData

        import Fw.Channel
        time get port timeGetOut

        @ Number of dropped packets
        telemetry DroppedPackets: U32 update on change

        @ Number of times a distinct APID could not be tracked because
        @ Samd21::FramerConfig::MAX_TRACKED_APIDS was already exceeded. Packets from an untracked
        @ APID still downlink normally, just without per-APID sequence count continuity.
        telemetry ApidOverflowCount: U8 update on change

    }
}

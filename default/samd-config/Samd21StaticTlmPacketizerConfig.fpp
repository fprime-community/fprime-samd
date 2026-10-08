module Samd21 {
    module StaticTlmPacketizerConfig {
        @ Size of StaticTlmPacketizer.pktSendIn. The port index is the packet id, so this
        @ must be strictly greater than the largest packet id driven from a port.
        constant NUM_TLM_PACKETS = 1

        @ Number of ports to multiplex the packet transmission over
        @ Every connected `pktSendOut` port will be invoked when a packet is sent
        @ The receivers of the packet should copy data out of the incoming buffer immediately
        constant NUM_PKT_OUT = 1
    }
}

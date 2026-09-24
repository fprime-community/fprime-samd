module Samd21 {
    @ Size of StaticTlmPacketizer.pktSendIn. The port index is the packet id, so this
    @ must be strictly greater than the largest packet id driven from a port.
    constant NUM_TLM_PACKETS = 1
}

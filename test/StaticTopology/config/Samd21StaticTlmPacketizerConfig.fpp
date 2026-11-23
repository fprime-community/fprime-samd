module Samd21 {
    module StaticTlmPacketizerConfig {
        @ Size of StaticTlmPacketizer.pktSendIn. The port index is the packet id, so this
        @ must be strictly greater than the largest packet id driven from a port.
        constant NUM_TLM_PACKETS = 1

        @ More than one output, with one left unconnected, so the tests cover the fan-out
        @ to every connected index and the skip of an unconnected one
        constant NUM_PKT_OUT = 3
    }
}

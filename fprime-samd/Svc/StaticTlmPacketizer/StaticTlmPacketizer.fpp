module Samd21 {
    @ static-tlm-packetizer
    @ A small bare-metal implementation on StaticTlmPacketizer
    passive component StaticTlmPacketizer {

        @ Send a telemetry packet
        sync input port pktSendIn: [Samd21.NUM_TLM_PACKETS] Svc.Sched

        @ Packet send port
        @ Ordered by Section, Group
        output port pktSendOut: Fw.Com

        @ Telemetry input port
        sync input port tlmRecvIn: Fw.Tlm

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Send a telemetry packet
        sync command SEND_PKT(
            $id: FwTlmPacketizeIdType  @< The packet ID
        ) \
            opcode 0

        @ Telemetry channel is not part of a telemetry packet.
        event NoChan(
            $id: FwChanIdType  @< The telemetry ID
        ) \
            severity warning low \
            id 0 \
            format "Telemetry ID 0x{x} not packetized"

        @ Couldn't find the packet to send
        event PacketNotFound(
            $id: FwTlmPacketizeIdType  @< The packet ID
        ) \
            severity warning low \
            id 1 \
            format "Could not find packet ID {}"

        @ Enables command handling
        import Fw.Command

        @ Enables event handling
        import Fw.Event

    }
}

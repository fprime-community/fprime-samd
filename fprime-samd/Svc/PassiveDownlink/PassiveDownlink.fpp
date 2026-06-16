module Samd21 {
    @ A component for packetizing downlink telemetry
    passive component PassiveDownlink {

        @ Port for receiving events
        sync input port LogRecv: Fw.Log

        @ Port for receiving telemetry values
        sync input port TlmRecv: Fw.Tlm

        @ FATAL event announce port
        output port FatalAnnounce: Svc.FatalEvent

        enum PacketPort {
            EVENT = 0,
            TELEMETRY = 1,
        }

        @ Packet send port
        output port PktSend: [2] Fw.Com

    }
}
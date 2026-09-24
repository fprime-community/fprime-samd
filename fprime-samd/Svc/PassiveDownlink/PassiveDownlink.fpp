module Samd21 {
    @ A component for packetizing downlink telemetry
    passive component PassiveDownlink {

        @ Port for receiving events
        sync input port LogRecv: Fw.Log

        @ FATAL event announce port
        output port FatalAnnounce: Svc.FatalEvent

        @ Packet send port
        output port PktSend: Fw.Com

    }
}

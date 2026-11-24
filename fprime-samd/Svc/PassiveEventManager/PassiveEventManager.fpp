module Samd21 {
    @ A component for packetizing downlink events
    passive component PassiveEventManager {

        @ Port for receiving events
        sync input port LogRecv: Fw.Log

        @ FATAL event announce port
        output port FatalAnnounce: Svc.FatalEvent

        @ Packet send port
        output port PktSend: Fw.Com

    }
}

module Samd21 {
    @ Power manager ComFramer with static memory
    passive component Framer {

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

    }
}
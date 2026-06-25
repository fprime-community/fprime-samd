module Samd21 {
    @ Power manager ComFramer with static memory
    passive component Framer {

        @ Input port that flushes the active Tx buffer to the driver
        guarded input port schedIn: Svc.Sched

        @ Input port that handles downlink packets
        guarded input port comPacketQueueIn: [ComQueueComPorts] Fw.Com

        @ Signal from the driver that it is ready
        sync input port drvConnected: Drv.ByteStreamReady

        @ Send a framed packet to the driver
        output port drvSendOut: Fw.BufferSend

        @ Receive buffer back from driver
        guarded input port drvReturnIn: Drv.ByteStreamData

    }
}
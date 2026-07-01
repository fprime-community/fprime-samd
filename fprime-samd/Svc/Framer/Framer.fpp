module Samd21 {
    @ Power manager ComFramer with static memory
    passive component Framer {

        @ Input port that flushes the active Tx buffer to the driver
        sync input port schedIn: Svc.Sched

        @ Input port that handles downlink packets
        sync input port comPacketQueueIn: [ComQueueComPorts] Fw.Com

        @ Input port that handles downlink packets synchronously
        sync input port comPacketSyncIn: [ComQueueComPorts] Fw.Com

        @ Signal from the driver that it is ready
        sync input port drvConnected: Drv.ByteStreamReady

        @ Send a framed packet to the driver
        output port drvSendOut: Fw.BufferSend

        @ Receive buffer back from driver
        sync input port drvReturnIn: Drv.ByteStreamData

        @ Send a framed packet to the driver and synchronously transmit
        output port drvSendSyncOut: Drv.ByteStreamSend

    }
}
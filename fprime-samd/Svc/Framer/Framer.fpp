module Samd21 {
    @ Power manager ComFramer with static memory
    passive component Framer {

        @ Input port that handles downlink packets
        guarded input port comPacketQueueIn: [ComQueueComPorts] Fw.Com

        @ Signal from the driver that it is ready
        sync input port drvConnected: Drv.ByteStreamReady

        @ Send a framed packet to the driver
        output port drvSendOut: Drv.ByteStreamSend

    }
}
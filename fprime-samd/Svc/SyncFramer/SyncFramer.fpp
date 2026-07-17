module Samd21 {
    @ A framer that transmits packets synchronously
    passive component SyncFramer {

        sync input port comPacketIn: Fw.Com

        @ Signal from the driver that it is ready
        sync input port drvConnected: Drv.ByteStreamReady

        @ Send a framed packet to the driver
        output port drvSendOut: Drv.ByteStreamSend

    }
}
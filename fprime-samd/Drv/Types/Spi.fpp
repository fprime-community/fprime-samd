module Samd21 {
    port SpiWriteRead(
        ref writeBuffer: Fw.Buffer
        ref readBuffer:  Fw.Buffer
    )

    port SpiReply(
        ref writeBuffer: Fw.Buffer
        ref readBuffer:  Fw.Buffer,
        status: Drv.SpiStatus
    )

    interface AsyncSpi {
        sync input port SpiWriteRead: [Samd21.SpiPorts] SpiWriteRead

        output port SpiReply: [Samd21.SpiPorts] SpiReply
    }

}

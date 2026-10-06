module Samd21 {
    port SpiWriteRead(
        ref writeBuffer: Fw.Buffer
        ref readBuffer:  Fw.Buffer
    )

    @ Completion of a SpiWriteRead. Invoked from the DMAC ISR (or synchronously from inside
    @ SpiWriteRead when the driver is busy). Do not issue the next SpiWriteRead from inside
    @ this call: the DMAC driver's channel bookkeeping has not finished yet -- defer it to
    @ main context.
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

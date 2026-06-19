module Samd21 {
    interface AsyncNonQueuedByteStreamDriver {
        @ Port invoked when the driver is ready to send/receive data
        output port ready: Drv.ByteStreamReady

        @ Port invoked by the driver when it receives data
        output port $recv: Drv.ByteStreamData

        @ Invoke this port to send data out the driver (asynchronous)
        @ Status and ownership of the buffer are returned through the sendReturnOut callback
        guarded input port $send: Fw.BufferSend @< send buffer to the DMAC synchronously

        @ Port returning ownership of data received on $send port
        output port sendReturnOut: Drv.ByteStreamData

        @ Port receiving back ownership of data sent out on $recv port
        guarded input port recvReturnIn: Fw.BufferSend
    }

    @ Universal Synchronous and Asynchronous Receiver and Transmitter Driver for SAMD21 SERCOM
    passive component UsartDriver {

        @ An enum for selecting the two DMA channels on the USART
        enum DmaChannel {
            TX,
            RX,
            N
        }

        import AsyncNonQueuedByteStreamDriver

        ####################################
        ## Buffer management ports
        ####################################

        @ Allocation port used for allocating memory in the receive task
        output port allocate: Fw.BufferGet

        @ Deallocation of allocated buffers
        output port deallocate: Fw.BufferSend

        ####################################
        ## DMA management ports
        ####################################

        @ Port to send a buffer to the UART DMA channel Tx line
        output port sendDmaOut: [DmaChannel.N] Dma.Transaction

        @ A signal from the DMAC that a request has finished.
        @ This signal comes inside an ISR!
        sync input port dmaReplyIn: [DmaChannel.N] Dma.TransactionReply

    }
}

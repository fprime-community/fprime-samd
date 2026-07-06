module Samd21 {
    interface AsyncNonQueuedByteStreamDriver {
        @ Port invoked when the driver is ready to send/receive data
        output port ready: Drv.ByteStreamReady

        @ Port invoked by the driver when it receives data
        output port $recv: Drv.ByteStreamData

        @ Invoke this port to send data out the driver (asynchronous)
        @ Status and ownership of the buffer are returned through the sendReturnOut callback
        sync input port $send: Fw.BufferSend @< send buffer to the DMAC synchronously

        @ Port returning ownership of data received on $send port
        output port sendReturnOut: Drv.ByteStreamData

        @ Port receiving back ownership of data sent out on $recv port
        sync input port recvReturnIn: Fw.BufferSend
    }

    @ Universal Synchronous and Asynchronous Receiver and Transmitter Driver for SAMD21 SERCOM
    passive component UsartDriver {

        @ An enum for selecting the two DMA channels on the USART
        enum DmaChannel {
            TX,     @< DMA channel for transmission
            RX,     @< DMA channel for receive
            N
        }

        import AsyncNonQueuedByteStreamDriver

        @ REQUIRED CONNECTION: Service the UART driver at a fixed rate for idle RX detection.
        @ This port MUST be connected to a rate group (e.g., 1Hz-8Hz).
        @ On each rate group tick, this handler:
        @ 1. Decrements the idle watchdog counter
        @ 2. Triggers partial RX frame extraction when the watchdog expires
        @ Without this connection, the driver can only receive full buffers.
        sync input port schedIn: Svc.Sched

        @ REQUIRED CONNECTION: Process the internal signal queue from DMA ISR handlers.
        @ This port MUST be connected to a PassiveCycler driven by the main loop.
        @ This handler:
        @ 1. Dequeues signals from DMA completion ISRs
        @ 2. Returns TX buffers with status to senders
        @ 3. Sends RX buffers downstream to consumers
        @ Without this connection, all TX/RX operations will hang.
        sync input port activeIn: Svc.Sched

        @ Synchronously transmit data on this UART
        @ Blocks until the entire buffer is transferred
        sync input port $sendSync: Drv.ByteStreamSend

        ####################################
        ## DMA management ports
        ####################################

        @ Port to send a DMA transaction to the DMA driver
        output port dmaQueueOut: [DmaChannel.N] Dma.Transaction

        @ Make the DMA channel for Rx a continuous circular chain
        output port dmaRxCircular: Fw.Signal

        @ Read the current DMA transfer on the Rx. This can be used during IDLE
        @ periods on the transfer to extract data that is smaller than a single Rx buffer
        output port dmaRxPopCurrent: Dma.ReadWriteback

        @ A signal from the DMAC that a request has finished.
        @ This signal comes inside an ISR!
        sync input port dmaReplyIn: [DmaChannel.N] Dma.TransactionReply

    }
}

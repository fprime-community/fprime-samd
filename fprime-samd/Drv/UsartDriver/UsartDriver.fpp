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
            TX,     @< DMA channel for transmission
            RX,     @< DMA channel for receive
            N
        }

        @ Signals sent to the internal queue for processing during schedIn
        enum Signal : U8 {
            TX_BUFFER_OK, @< A buffer has been TXed over the DMA successfully
            TX_BUFFER_ERROR, @< A buffer failed to be transmitted due to a bus error
            RX_BUFFER_FULL, @< An RX buffer has been filled and is ready for processing
            RX_BUFFER_PARTIAL, @< A partial RX buffer is ready for processing
            RX_BUFFER_ERROR, @< An error occurred while filling an RX buffer
        }

        import AsyncNonQueuedByteStreamDriver

        @ Service the UART driver at a fixed rate:
        @
        @ 1. Return Tx buffers that finished DMA transmission.
        @ 2. Detect IDLE Rx. Pull the in-progress DMA Rx and send it downstream
        sync input port schedIn: Svc.Sched

        @ Unload the event loop to service any requests coming from interrupts
        sync input port activeIn: Svc.Sched

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

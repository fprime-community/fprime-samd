module Samd21 {
    @ Driver for the SAMD21 SPI Peripheral
    passive component SpiDriver {

        @ An enum for selecting the two DMA channels on the USART
        enum DmaChannel {
            MOSI,     @< DMA channel for transmission
            MISO,     @< DMA channel for receive
            N
        }

        @ Note! The SpiReply signal is sent on an ISR
        import Samd21.AsyncSpi

        @ Port to send a DMA transaction to the DMA driver
        output port dmaTransactionOut: [DmaChannel.N] Dma.Transaction

        @ A signal from the DMAC that a request has finished.
        @ This signal comes inside an ISR!
        sync input port dmaReplyIn: [DmaChannel.N] Dma.TransactionReply

    }
}
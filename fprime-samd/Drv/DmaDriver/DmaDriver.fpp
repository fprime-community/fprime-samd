module Samd21 {

    @ A component for managing DMA channels on the SAMD21
    passive component DmaDriver {

        @ Queue a DMA transaction on a channel.
        @ If the channel is idle, the transaction starts immediately.
        @ If the channel is busy, the transaction is appended to the linked descriptor chain.
        @ Invalid parameters (alignment, length, null addresses) trigger assertions.
        guarded input port sendTransactionIn: [Dma.CHANNEL_NUM] Dma.Transaction

        @ Suspend a DMA channel.
        @ This will trigger suspendIsrOut after the DMA channel has finished transferring its current block
        guarded input port suspendIn: [Dma.CHANNEL_NUM] Fw.Signal

        @ Read the writeback register on the active DMA transfer for this channel
        sync input port readWritebackIn: [Dma.CHANNEL_NUM] Dma.ReadWriteback

        @ Signal from ISR that a DMA channel has been suspended
        output port suspendIsrOut: [Dma.CHANNEL_NUM] Fw.Signal

        @ Signal from ISR that a DMA transaction has completed (success or error)
        output port transactionIsrOut: [Dma.CHANNEL_NUM] Dma.TransactionReply

        # The port number determines which channel is being configured/started

        match transactionIsrOut with sendTransactionIn
        match suspendIsrOut with suspendIn
    }
}
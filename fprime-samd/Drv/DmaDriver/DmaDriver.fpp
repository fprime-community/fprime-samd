module Samd21 {

    @ A component for managing DMA channels on the SAMD21
    passive component DmaDriver {

        @ Queue a DMA transaction on a channel.
        @ If the channel is idle, the transaction starts immediately.
        @ If the channel is busy, the transaction is appended to the linked descriptor chain.
        @ Invalid parameters (alignment, length, null addresses) trigger assertions.
        sync input port sendTransactionIn: [Dma.CHANNEL_NUM] Dma.Transaction

        @ Updates the final DmaDescriptor in the chain configured on this channel to link
        @ back to the first. This allows this channel to loop to the first transaction continuously.
        sync input port linkToFrontIn: [Dma.CHANNEL_NUM] Fw.Signal

        @ Read the writeback register on the active DMA transfer for this channel
        @ 1. Suspend the channel (if not already suspended)
        @ 2. Wait for CHINTFLAG.SUSP to confirm suspension
        @ 3. Read writeback to get current state
        @ 4. Resume the channel
        sync input port readWritebackIn: [Dma.CHANNEL_NUM] Dma.ReadWriteback

        @ Signal from ISR that a DMA transaction has completed (success or error)
        output port transactionIsrOut: [Dma.CHANNEL_NUM] Dma.TransactionReply

        import Fw.Channel
        time get port timeCaller

        sync input port schedIn: Svc.Sched
        telemetry freeDescriptors: U8
    }
}
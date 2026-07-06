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

        @ Suspend the channel, read writeback, and advance to the next descriptor.
        @ Used for IDLE frame detection: process partial buffer and move to alternate.
        @
        @ Algorithm:
        @ 1. Suspend the channel (if not already suspended)
        @ 2. Wait for CHINTFLAG.SUSP to confirm suspension
        @ 3. Read writeback to get current state
        @ 4. User calculates bytes received (original_BTCNT - writeback.btcnt)
        @ 5. Skip to next descriptor by setting writeback BTCNT to 0
        @ 6. Resume the channel
        @
        @ Returns: Writeback state at suspend point
        sync input port popFrontIn: [Dma.CHANNEL_NUM] Dma.ReadWriteback

        @ Read the writeback register on the active DMA transfer for this channel
        sync input port readWritebackIn: [Dma.CHANNEL_NUM] Dma.ReadWriteback

        @ Signal from ISR that a DMA transaction has completed (success or error)
        output port transactionIsrOut: [Dma.CHANNEL_NUM] Dma.TransactionReply

        struct Transaction {
            @ DMA controller trigger source
            trigger: Dma.TriggerSource,
            @ DMA controller behavior on this channel given memory and a trigger source
            $action: Dma.TransactionType,
            @ DMA transaction priority
            $priority: Dma.Priority,
            @ The source address to move data from
            sourceAddr: U32, # FIXME(tumbar) Is there a alias type I should be using for pointers?
            @ The destination address to move data to
            destAddr: U32,
            @ Number of bytes to copy from source to destination
            len: U32,
            @ Size of each beat. Controls the width of each DMA action
            beatSize: Dma.BeatSize,
            @ Whether the DMA controller should increment the source pointer after each action signal
            incrementSource: bool,
            @ Whether the DMA controller should increment the destination pointer after each action signal
            incrementDestination: bool,
            @ Size to increment address (source/dest when enabled) on each beat
            stepSize: Dma.AddressIncrementStepSize,
            @ Determines whether the step size setting is applied to the source or destination address
            stepSelection: Dma.StepSelection,
        }

        event Transaction (
            channel: U32,
            descriptor_index: U32,
            transaction: Transaction
        ) severity activity low format "Queueing DMA transaction on channel {} with descriptor index {}: {}"

        event ChannelCirculuar(
            channel: U32
        ) severity activity low format "Linking DMA channel {} into a circular mode"

        time get port timeCaller
        import Fw.Event
    }
}
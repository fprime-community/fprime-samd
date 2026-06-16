module Samd21 {

    @ Port for configuring a DMA channel
    port DmaRegister(
        @ DMA controller trigger source
        trigger: DmaDriver.TriggerSource,
        @ DMA controller behavior on this channel given memory and a trigger source
        $action: DmaDriver.TransactionType,
        @ DMA transaction priority
        $priority: DmaDriver.Priority,
    )

    @ Port for adding a transaction descriptor to a DMA channel
    port DmaAddDescriptor(
        @ The source address to move data from
        sourceAddr: U32, # FIXME(tumbar) Is there a alias type I should be using for pointers?
        @ The destination address to move data to
        destAddr: U32,
        @ Number of bytes to copy from source to destination
        len: U32,
        @ Size of each beat. Controls the width of each DMA action
        beatSize: DmaDriver.BeatSize,
        @ Whether the DMA controller should increment the source pointer after each action signal
        incrementSource: bool,
        @ Whether the DMA controller should increment the destination pointer after each action signal
        incrementDestination: bool,
        @ Size to increment address (source/dest when enabled) on each beat
        stepSize: DmaDriver.AddressIncrementStepSize,
        @ Determines whether the step size setting is applied to the source or destination address
        stepSelection: DmaDriver.StepSelection,
    )

    @ Used for accessing the writeback descriptor for peaking into DMA for Rx
    struct DmaWriteback {
        
    }

    @ Read the writeback descriptor of a DMA channel.
    @ This is updated live during DMA transactions.
    port DmaGetWriteback() -> DmaWriteback

    @ A component for managing DMA channels on the SAMD21
    passive component DmaDriver {
        @ Number of DMAC channels on the SAMD21
        # FIXME(tumbar) Does this need to be in the configuration?
        constant DMAC_CHANNEL_NUM = 12

        @ Size of each DMAC beat
        enum BeatSize: U8 {
            BYTE  @< 8-bit
            HWORD @< 16-bit
            WORD  @< 32-bit
        }

        @ 
        enum TransactionType: U8 {
            BEAT,           @< One data transfer bus access per trigger
            BLOCK,          @< 1kB to 64kB per trigger
            TRANSACTION,    @< Perform the entire transaction on a signal trigger
        }

        enum TriggerSource: U8 {
            DISABLE = 0x00,     @< Only software/event triggers
            SERCOM0_RX = 0x01,  @< SERCOM0 RX Trigger
            SERCOM0_TX = 0x02,  @< SERCOM0 TX Trigger
            SERCOM1_RX = 0x03,  @< SERCOM1 RX Trigger
            SERCOM1_TX = 0x04,  @< SERCOM1 TX Trigger
            SERCOM2_RX = 0x05,  @< SERCOM2 RX Trigger
            SERCOM2_TX = 0x06,  @< SERCOM2 TX Trigger
            SERCOM3_RX = 0x07,  @< SERCOM3 RX Trigger
            SERCOM3_TX = 0x08,  @< SERCOM3 TX Trigger
            SERCOM4_RX = 0x09,  @< SERCOM4 RX Trigger
            SERCOM4_TX = 0x0A,  @< SERCOM4 TX Trigger
            SERCOM5_RX = 0x0B,  @< SERCOM5 RX Trigger
            SERCOM5_TX = 0x0C,  @< SERCOM5 TX Trigger
            TCC0_OVF = 0x0D,    @< TCC0 Overflow Trigger
            TCC0_MC0 = 0x0E,    @< TCC0 Match/Compare 0 Trigger
            TCC0_MC1 = 0x0F,    @< TCC0 Match/Compare 1 Trigger
            TCC0_MC2 = 0x10,    @< TCC0 Match/Compare 2 Trigger
            TCC0_MC3 = 0x11,    @< TCC0 Match/Compare 3 Trigger
            TCC1_OVF = 0x12,    @< TCC1 Overflow Trigger
            TCC1_MC0 = 0x13,    @< TCC1 Match/Compare 0 Trigger
            TCC1_MC1 = 0x14,    @< TCC1 Match/Compare 1 Trigger
            TCC2_OVF = 0x15,    @< TCC2 Overflow Trigger
            TCC2_MC0 = 0x16,    @< TCC2 Match/Compare 0 Trigger
            TCC2_MC1 = 0x17,    @< TCC2 Match/Compare 1 Trigger
            TC3_OVF = 0x18,     @< TC3 Overflow Trigger
            TC3_MC0 = 0x19,     @< TC3 Match/Compare 0 Trigger
            TC3_MC1 = 0x1A,     @< TC3 Match/Compare 1 Trigger
            TC4_OVF = 0x1B,     @< TC4 Overflow Trigger
            TC4_MC0 = 0x1C,     @< TC4 Match/Compare 0 Trigger
            TC4_MC1 = 0x1D,     @< TC4 Match/Compare 1 Trigger
            TC5_OVF = 0x1E,     @< TC5 Overflow Trigger
            TC5_MC0 = 0x1F,     @< TC5 Match/Compare 0 Trigger
            TC5_MC1 = 0x20,     @< TC5 Match/Compare 1 Trigger
            TC6_OVF = 0x21,     @< TC6 Overflow Trigger
            TC6_MC0 = 0x22,     @< TC6 Match/Compare 0 Trigger
            TC6_MC1 = 0x23,     @< TC6 Match/Compare 1 Trigger
            TC7_OVF = 0x24,     @< TC7 Overflow Trigger
            TC7_MC0 = 0x25,     @< TC7 Match/Compare 0 Trigger
            TC7_MC1 = 0x26,     @< TC7 Match/Compare 1 Trigger
            ADC_RESRDY = 0x27,  @< ADC Result Ready Trigger
            DAC_EMPTY = 0x28,   @< DAC Empty Trigger
            I2S_RX0 = 0x29,     @< I2S RX 0 Trigger
            I2S_RX1 = 0x2A,     @< I2S RX 1 Trigger
            I2S_TX0 = 0x2B,     @< I2S TX 0 Trigger
            I2S_TX1 = 0x2C,     @< I2S TX 1 Trigger
            TCC3_OVF = 0x2D,    @< TCC3 Overflow Trigger
            TCC3_MC0 = 0x2E,    @< TCC3 Match/Compare 0 Trigger
            TCC3_MC1 = 0x2F,    @< TCC3 Match/Compare 1 Trigger
            TCC3_MC2 = 0x30,    @< TCC3 Match/Compare 2 Trigger
            TCC3_MC3 = 0x31     @< TCC3 Match/Compare 3 Trigger
        }

        @ Size of the address increment
        enum AddressIncrementStepSize: U8 {
            SIZE_1 = 0,
            SIZE_2 = 1,
            SIZE_4 = 2,
            SIZE_8 = 3,
            SIZE_16 = 4,
            SIZE_32 = 5,
            SIZE_64 = 6,
            SIZE_128 = 7,
        }

        @ This bit determines whether the step size setting is applied to the source or destination address
        enum StepSelection: U8 {
            DESTINATION = 0,
            SOURCE = 1,
        }

        enum Priority {
            PRIORITY_0, @< Lowest priority (default)
            PRIORITY_1,
            PRIORITY_2,
            PRIORITY_3, @< Highest priority
        }

        @ Configure the DMA controller on a channel determined by the port number
        @ If this channel was already configured, overwrite the previous configuration
        sync input port configure: [DMAC_CHANNEL_NUM] DmaRegister

        @ Start DMA on a configured channel
        @ The channel must already be configured before this is done
        sync input port addDescriptor: [DMAC_CHANNEL_NUM] DmaAddDescriptor

        @ Suspend a DMA channel.
        @ This will trigger a suspendOut after the DMA channel has finished transfering it's current block
        sync input port suspendIn: [DMAC_CHANNEL_NUM] Fw.Signal

        @ Signal back from the DMAC that the 
        output port suspendOutIsr: [DMAC_CHANNEL_NUM] Fw.Signal

        @ Signal back from the DMAC that the DMA transfer is complete
        output port finishedIsr: [DMAC_CHANNEL_NUM] Fw.Signal

        # The port number determines which channel is being configured/started

        match finishedIsr with configure
        match finishedIsr with start

        match suspendOutIsr with suspendIn
    }
}
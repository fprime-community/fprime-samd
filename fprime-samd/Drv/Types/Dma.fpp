module Samd21 {

    module Dma {
        # FIXME(tumbar) Does this need to be in the configuration?
        @ Number of DMAC channels on the SAMD21
        constant CHANNEL_NUM = 12

        @ Size of each DMAC beat
        enum BeatSize: U8 {
            BYTE  @< 8-bit
            HWORD @< 16-bit
            WORD  @< 32-bit
        }

        @ Action the DMAC should perform on a single trigger on the channel
        enum TransactionType: U8 {
            BLOCK = 0x0,       @< One trigger required for each block transfer
            BEAT = 0x2,        @< One trigger required for each beat transfer
            TRANSACTION = 0x3, @< One trigger required for each transaction
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

        @ Port for adding a transaction descriptor to a DMA channel
        port Transaction(
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
        )

        @ DMA transaction completion status
        enum Status {
            OK,         @< Transaction completed successfully
            BUS_ERROR,  @< AHB bus error during transfer (hardware fault, invalid address, MPU violation)
        }

        @ DMA transaction completion reply from ISR
        struct Reply {
            $status: Dma.Status,          @< Whether transaction completed successfully or had an error
            remainingBytes: U32,        @< Bytes NOT transferred (0 if OK, >0 if BUS_ERROR)
        }

        @ Reply port for DMA transaction completion
        port TransactionReply(
            reply: Dma.Reply
        )

        @ Used for accessing the writeback descriptor for peaking into DMA for Rx
        struct Writeback {
            btctrl: U16,        @< Block Transfer Control (read-only copy)
            btcnt: U16,         @< Remaining beat count
            srcaddr: U32,       @< Current source address
            dstaddr: U32,       @< Current destination address
            descaddr: U32,      @< Next descriptor address
        }

        @ Read the writeback descriptor of a DMA channel.
        @ This is updated live during DMA transactions.
        port ReadWriteback() -> Writeback
    }

}
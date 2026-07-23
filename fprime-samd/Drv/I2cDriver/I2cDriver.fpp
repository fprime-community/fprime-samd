module Samd21 {
    interface AsyncSyncI2c {
        # ----------------------------------------------------------------------
        # I2C interface ports (async with callbacks)
        # ----------------------------------------------------------------------

        @ Port for asynchronous write transaction
        sync input port write: Drv.I2cRequest

        @ Port for asynchronous read transaction
        sync input port read: Drv.I2cRequest

        @ Port for asynchronous write-read transaction
        sync input port writeRead: Drv.I2cWriteReadRequest

        ###### Ports below must be connected if buffers are being passed to/from i2c drv ######

        @ Port invoked when write transaction completes
        output port writeComplete: Drv.I2cCallback

        @ Port invoked when read transaction completes
        output port readComplete: Drv.I2cCallback

        @ Port invoked when write-read transaction completes
        output port writeReadComplete: Drv.I2cWriteReadCallback
    }

    @ Driver for the SAMD21 I2C Peripheral
    passive component I2cDriver {

        import AsyncSyncI2c

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables telemetry channels handling
        import Fw.Channel

        @ Port to send a DMA transaction to the DMA driver
        output port dmaTransactionOut: Dma.Transaction

        @ A signal from the DMAC that a request has finished.
        @ This signal comes inside an ISR!
        sync input port dmaReplyIn: Dma.TransactionReply

    }
}
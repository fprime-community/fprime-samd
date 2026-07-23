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

        @ Note: All complete replies come back either on the SERCOM ISR Handler for 
        import AsyncSyncI2c

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables event handling
        import Fw.Event

        @ Port to send a DMA transaction to the DMA driver
        output port dmaTransactionOut: Dma.Transaction

        @ Port to abort DMA transactions on a specific DMA channel
        output port dmaTransactionAbortOut: Fw.Signal

        @ A signal from the DMAC that a request has finished.
        @ This signal comes inside an ISR!
        sync input port dmaReplyIn: Dma.TransactionReply

        enum I2cHostError : U8 {
            @ Automatic length that is used for a DMA transaction and the client sends a NACK
            @ before ADDR.LEN bytes have been written by the host.
            LengthError,

            @ Slave SCL low extend time-out occured.
            SlaveSclExtendTimeout,

            @ Master SCL low extend time-out occured.
            MasterSclExtendTimeout,

            @ SCL low time-out occured.
            SclLowTimeout,

            @ Arbitration was lost while transmitting a high data bit or a NACK bit, or while issuing a
            @ Start or Repeated Start condition on the bus. The Host on Bus Interrupt flag (INTFLAG.MB) will be set
            @ when STATUS.ARBLOST is set.
            ArbitrationLost,

            @ This bit indicates that an illegal Bus condition has occurred on the bus, regardless of bus ownership.
            @ An illegal Bus condition is detected if a protocol violating start, repeated start or stop is detected on
            @ the I2C bus lines. A Start condition directly followed by a Stop condition is one example of a protocol
            @ violation. If a time-out occurs during a frame, this is also considered a protocol violation, and will set
            @ BUSERR.
            BusError
        }

        event I2cBusError(sercom: SercomKind, err: I2cHostError) severity warning low \
            id 0 \
            format "{} I2C Master error: {}"
    }
}
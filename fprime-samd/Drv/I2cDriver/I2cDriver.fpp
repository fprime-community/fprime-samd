module Samd21 {
    interface AsyncSyncI2c {
        # ----------------------------------------------------------------------
        # I2C interface ports (async with callbacks)
        # ----------------------------------------------------------------------

        @ Port for asynchronous write transaction
        sync input port write: [Samd21.I2cClientPorts] Drv.I2cRequest

        @ Port for asynchronous read transaction
        sync input port read: [Samd21.I2cClientPorts] Drv.I2cRequest

        @ Port for asynchronous write-read transaction
        sync input port writeRead: [Samd21.I2cClientPorts] Drv.I2cWriteReadRequest

        ###### Ports below must be connected if buffers are being passed to/from i2c drv ######

        @ Port invoked when write transaction completes
        output port writeComplete: [Samd21.I2cClientPorts] Drv.I2cCallback

        @ Port invoked when read transaction completes
        output port readComplete: [Samd21.I2cClientPorts] Drv.I2cCallback

        @ Port invoked when write-read transaction completes
        output port writeReadComplete: [Samd21.I2cClientPorts] Drv.I2cWriteReadCallback
    }

    @ Driver for the SAMD21 I2C Peripheral
    passive component I2cDriver {
        @ An enum for selecting the two DMA channels on the USART
        enum DmaChannel: U8 {
            WRITE @< DMA channel for write transactions
            READ  @< DMA channel for read transactions
            N
        }

        @ Note: All complete replies come back either on the SERCOM ISR Handler for
        import AsyncSyncI2c

        match writeComplete with write
        match readComplete with read
        match writeReadComplete with writeRead

        ###############################################################################
        # Standard AC Ports: Required for Channels, Events, Commands, and Parameters  #
        ###############################################################################
        @ Port for requesting the current time
        time get port timeCaller

        @ Enables event handling
        import Fw.Event

        @ Enables telemetry handling
        import Fw.Channel

        @ Enables command handling
        import Fw.Command

        @ Port for periodically writing telemetry
        sync input port reportTelemetryIn: Svc.Sched

        @ REQUIRED CONNECTION: deliver transaction completions from the main context.
        @ Connect to a PassiveCycler. The completion callbacks (read/write/writeRead
        @ Complete) are driven from here rather than from the DMA/SERCOM ISR, so client
        @ components run outside interrupt context. Returns true if a completion was
        @ delivered this tick (so the cycler re-invokes us), false when idle.
        sync input port activeIn: Svc.ActiveSched

        @ Port to send a DMA transaction to the DMA driver
        output port dmaTransactionOut: [DmaChannel.N] Dma.Transaction

        @ Port to abort DMA transactions on a specific DMA channel
        output port dmaTransactionAbortOut: [DmaChannel.N] Fw.Signal

        @ A signal from the DMAC that a request has finished.
        @ This signal comes inside an ISR!
        sync input port dmaReplyIn: [DmaChannel.N] Dma.TransactionReply

        enum I2cError: U8 {
            @ Automatic length that is used for a DMA transaction and the client sends a NACK
            @ before ADDR.LEN bytes have been written by the host.
            LENGTH_ERROR

            @ Slave SCL low extend time-out occured.
            SLAVE_SCL_EXTEND_TIMEOUT

            @ Master SCL low extend time-out occured.
            MASTER_SCL_EXTEND_TIMEOUT

            @ SCL low time-out occured.
            SCL_LOW_TIMEOUT

            @ Arbitration was lost while transmitting a high data bit or a NACK bit, or while issuing a
            @ Start or Repeated Start condition on the bus. The Host on Bus Interrupt flag (INTFLAG.MB) will be set
            @ when STATUS.ARBLOST is set.
            ARBITRATION_LOST

            @ This bit indicates that an illegal Bus condition has occurred on the bus, regardless of bus ownership.
            @ An illegal Bus condition is detected if a protocol violating start, repeated start or stop is detected on
            @ the I2C bus lines. A Start condition directly followed by a Stop condition is one example of a protocol
            @ violation. If a time-out occurs during a frame, this is also considered a protocol violation, and will set
            @ BUSERR.
            BUS_ERROR
        }

        event I2cBusError(sercom: SercomKind, err: I2cError) \
            severity warning low \
            id 0 \
            format "{} I2C Master error: {}"

        enum I2CInterrupt: U8 {
            MASTER_ON_BUS
            BUS_ERROR
        }

        event UnexpectedInterrupt(sercom: SercomKind, err: I2CInterrupt) \
            severity warning high \
            id 1 \
            format "I2C {} raised a {} interrupt in an expected IDLE state"

        event InvalidDmaReply(
            sercom: SercomKind
            $channel: DmaChannel
            expected: DmaChannel
        ) \
            severity warning high \
            id 2 \
            format "I2C {} got a DMA reply from {} while expecting a reply from {}"

        @ The stall watchdog found a transaction that never completed and force-
        @ recovered the peripheral. The fields are the frozen INTFLAG/STATUS
        @ registers captured before recovery, so the wedge can be diagnosed after
        @ the fact. state is the driver State the transaction was stuck in.
        event StalledTransactionRecovered(
            sercom: SercomKind
            stuckState: U8
            intflag: U8
            status: U16
        ) \
            severity warning high \
            id 3 \
            format "I2C {} recovered a stalled transaction in state {}: INTFLAG=0x{x} STATUS=0x{x}"

        @ Running count of bus errors reported by this I2C peripheral
        telemetry BusErrorCount: U32 id 0

        enum I2cBusState: U8 {
            UNKNOWN = 0
            IDLE    = 1
            OWNER   = 2
            BUSY    = 3
        } default UNKNOWN

        @ Current I2c bus state
        telemetry BusState: I2cBusState id 1

        @ Is the host holding the SCL waiting on the software/DMA
        telemetry ClockHold: bool id 2

        @ Did the client ACK the read address or NACK?
        telemetry ReceiveNotAcknowledged: bool id 3

        enum DeviceOnBusFlag: U8 {
            NONE
            MASTER_ON_BUS
            SLAVE_ON_BUS
            MASTER_AND_SLAVE_ON_BUS
        }

        @ I2C Bus interrupt bit flags
        telemetry DeviceOnBus: DeviceOnBusFlag id 4

        @ Running count of stalled transactions force-recovered by the watchdog
        telemetry StallRecoveryCount: U32 id 5

        @ Clear the [BusErrorCount] channel
        sync command CLEAR_ERRORS opcode 0
    }
}

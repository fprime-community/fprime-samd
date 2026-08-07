# Samd21::I2cDriver

Driver for the SAMD21 SERCOM peripheral in I2C Host (master) mode.

## 1. Introduction

The `Samd21::I2cDriver` component drives a SAMD21 SERCOM peripheral as an I2C
host (master). It is a passive, DMA-driven component: the data payload of every
transaction is moved by the DMA controller rather than the CPU, and the driver
runs without an RTOS thread.

The driver exposes three asynchronous operations — `write`, `read`, and
`writeRead` — each with a matching completion callback. A `writeRead` is a
combined transaction that issues a repeated START between the write and the read
(no STOP in between), so the addressed device keeps its internal register pointer
across the two phases. This is the access pattern register-based devices such as
the LTC2945 power monitor require.

Because there is no RTOS, transaction completions are delivered from interrupt
context; there is no cycler or main-loop tick in the driver's critical path.

## 2. Requirements

| Name           | Description                                                                                                                                                            | Validation    |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------- |
| SAMD21-I2C-001 | The I2cDriver shall configure a SERCOM peripheral for I2C host operation with configurable SCL frequency (100kHz / 400kHz / 1MHz / 3.4MHz), SDA hold, and pin usage.  | Hardware Test |
| SAMD21-I2C-002 | The I2cDriver shall perform 7-bit-addressed write, read, and combined write-read transactions using DMA for the data payload.                                          | Hardware Test |
| SAMD21-I2C-003 | The I2cDriver shall implement `writeRead` as a repeated START with no intervening STOP.                                                                                | Hardware Test |
| SAMD21-I2C-004 | The I2cDriver shall report bus errors as events, count them in telemetry, and fail the in-flight transaction with the appropriate status.                              | Hardware Test |
| SAMD21-I2C-005 | The I2cDriver shall reject a new request that arrives while a transaction is in progress, returning `I2C_OTHER_ERR` on the matching completion callback.               | Hardware Test |

## 3. Design

### 3.1 Overview

`Samd21::I2cDriver` is a passive component presenting the `AsyncSyncI2c`
interface: three request ports (`write`, `read`, `writeRead`) with matching
completion callbacks. Only one transaction may be in flight at a time; a request
that arrives while the driver is busy is rejected immediately on its completion
callback with `I2C_OTHER_ERR`.

The data payload is transferred by the `Samd21::DmaDriver` over **two DMA
channels** — one for transmit, one for receive. Two channels are required because
a DMA channel has a single trigger source, and the write and read phases use
different SERCOM triggers. Using separate channels also lets the read transfer be
armed ahead of time so the write→read transition never has to set up DMA from
within an interrupt.

Transaction completions (and bus errors) are delivered to the driver from
interrupt context and are forwarded straight to the caller's completion callback.
The driver does not use a signal queue or `PassiveCycler`, so **completion
callbacks run in ISR context** — consumers must keep those handlers short.

### 3.2 Ports

| Kind         | Name                        | Port Type                  | Usage                                                      |
| ------------ | --------------------------- | -------------------------- | ---------------------------------------------------------- |
| `sync input` | `write[2]`                  | `Drv.I2cRequest`           | Start a write transaction                                  |
| `sync input` | `read[2]`                   | `Drv.I2cRequest`           | Start a read transaction                                   |
| `sync input` | `writeRead[2]`              | `Drv.I2cWriteReadRequest`  | Start a combined write-then-read (repeated START)          |
| `output`     | `writeComplete[2]`          | `Drv.I2cCallback`          | Write completion + status                                  |
| `output`     | `readComplete[2]`           | `Drv.I2cCallback`          | Read completion + status                                   |
| `output`     | `writeReadComplete[2]`      | `Drv.I2cWriteReadCallback` | Write-read completion + status                             |
| `output`     | `dmaTransactionOut[N]`      | `Dma.Transaction`          | Queue a DMA transfer on the WRITE or READ channel          |
| `output`     | `dmaTransactionAbortOut[N]` | `Fw.Signal`                | Abort a channel's DMA transfer (error teardown)            |
| `sync input` | `dmaReplyIn[N]`             | `Dma.TransactionReply`     | DMA completion from the DMAC (ISR context)                 |
| `sync input` | `reportTelemetryIn`         | `Svc.Sched`                | Periodic tick that emits bus-state / error telemetry       |
| `time get`   | `timeCaller`                | —                          | Timestamp source for telemetry                             |

The DMA ports are arrayed and indexed by a `DmaChannel` enum (`WRITE`, `READ`).
Each index must be wired to a distinct physical DMA channel in the topology.

### 3.3 Configuration Options

`configure()` sets up the SERCOM once at startup. The caller selects:

| Option              | Choices                                                              |
| ------------------- | -------------------------------------------------------------------- |
| SERCOM instance     | `SERCOM_0` … `SERCOM_5` (device-dependent)                           |
| SCL frequency       | 100 kHz, 400 kHz, 1 MHz, or 3.4 MHz                                  |
| SDA hold time       | Disabled, 75 ns, 450 ns, or 600 ns                                   |
| Pin usage           | Two-wire (SCL/SDA) or four-wire                                      |
| Clock stretch mode  | Always, or only after ACK                                            |
| SMBus time-outs     | SCL-low, inactive-bus, host SCL-extend, client SCL-extend (each on/off) |
| Run in standby      | Enabled or disabled                                                 |

The driver uses 7-bit addressing and transfers up to 255 bytes per transaction.

**Clocking.** The SERCOM core clock (which sets the SCL baud rate) is taken from
the 48 MHz main clock generator. The SMBus time-outs are clocked by the separate,
SERCOM-wide "slow" clock, which the driver routes from a 32.768 kHz generator. If
the slow clock is not present, any enabled time-out never fires — so the driver
always configures it during `configure()`. On this project that 32.768 kHz
generator is sourced from the internal oscillator on bench (crystal-less) builds
and from the external crystal on flight builds; the driver does not need to know
which.

### 3.4 Transaction Behavior

- **Write** — the driver transmits the buffer to the device and completes with a
  STOP.
- **Read** — the driver reads the requested number of bytes from the device,
  ending with a NACK + STOP.
- **Write-read** — the driver transmits the write buffer, then issues a repeated
  START and reads the requested bytes, ending with a NACK + STOP. No STOP occurs
  between the two phases, so the device retains its register pointer. The read
  transfer is armed before the write completes, and the transition from writing
  to reading is driven by the SERCOM's "master on bus" interrupt so that the read
  START is issued at the correct moment (after the final write byte is on the
  bus, while the clock is held).

Each transaction ends by invoking the matching completion callback with a status
(`I2C_OK` on success, or an error status). Completions are delivered from
interrupt context.

### 3.5 Error Handling

The driver enables the SERCOM error interrupt. When a bus error is detected
(bus error, arbitration lost, an SMBus time-out, or a transaction-length error),
the driver:

- emits an `I2cBusError` event identifying the error,
- increments the bus-error telemetry counter, and
- tears down the in-flight transaction (aborting its DMA) and fires the matching
  completion callback with an error status.

Bus errors most often stem from the physical bus — missing or weak SDA/SCL
pull-ups, a line held low, or an enabled SMBus time-out tripping on a stalled
bus — so they are the first thing to check when a transaction fails.

For a `writeRead`, the driver also checks that the device acknowledged the write
(register-pointer) phase before issuing the repeated START. If the device NACKs,
the write-read is failed with a write error and the read is not attempted.

A DMA completion that does not match the expected channel/state is reported via
the `InvalidDmaReply` / `UnexpectedInterrupt` events and fails the transaction
rather than asserting.

### 3.6 Telemetry, Events, and Commands

| Kind      | Name                     | Description                                              |
| --------- | ------------------------ | -------------------------------------------------------- |
| Telemetry | `I2cBusErrorFlags`       | Running count of detected bus errors                     |
| Telemetry | `BusState`               | Current bus state (unknown / idle / owner / busy)        |
| Telemetry | `ClockHold`              | Host is holding SCL waiting on software/DMA              |
| Telemetry | `ReceiveNotAcknowledged` | Last address/data byte was NACKed                        |
| Telemetry | `DeviceOnBus`            | Master/client on-bus status                              |
| Event     | `I2cBusError`            | A bus error was detected (warning/low)                   |
| Event     | `Transaction`            | Trace of each read/write kick-off (activity/low)         |
| Event     | `UnexpectedInterrupt`    | Interrupt taken in an unexpected state (warning/high)    |
| Event     | `InvalidDmaReply`        | DMA reply for the wrong channel/state (warning/high)     |
| Command   | `CLEAR_ERRORS`           | Reset the bus-error telemetry counter                    |

## 4. Integration

### 4.1 Initialization

Configure the SDA/SCL GPIO pin muxing (via `Samd21::PinMux::configure()`) **before**
calling `configure()`. `configure()` may be called only once, and requires the
`Samd21::DmaDriver` to be configured as well since the driver relies on it for all
transfers.

For a single-master bus during bring-up, leaving the SMBus time-outs disabled is
the simplest starting point. The SCL-low time-out is worth enabling in operation
for automatic stuck-bus recovery, at the cost of a reported bus error whenever it
trips.

### 4.2 Topology Connections

Each DMA channel needs its own physical DMAC channel wired for transactions,
aborts, and completion replies. The device component connects to whichever
request/callback ports it uses (a register-based sensor typically uses only
`writeRead`).

```fpp
enum DmaChannel : U8 {
  # ... other channels ...
  SERCOM4_I2C_WRITE,
  SERCOM4_I2C_READ,
}

connections I2c {
  # WRITE channel
  i2cDriver.dmaTransactionOut[Samd21.I2cDriver.DmaChannel.WRITE]
      -> dmaDriver.sendTransactionIn[DmaChannel.SERCOM4_I2C_WRITE]
  i2cDriver.dmaTransactionAbortOut[Samd21.I2cDriver.DmaChannel.WRITE]
      -> dmaDriver.abortTransactionIn[DmaChannel.SERCOM4_I2C_WRITE]
  dmaDriver.transactionIsrOut[DmaChannel.SERCOM4_I2C_WRITE]
      -> i2cDriver.dmaReplyIn[Samd21.I2cDriver.DmaChannel.WRITE]

  # READ channel
  i2cDriver.dmaTransactionOut[Samd21.I2cDriver.DmaChannel.READ]
      -> dmaDriver.sendTransactionIn[DmaChannel.SERCOM4_I2C_READ]
  i2cDriver.dmaTransactionAbortOut[Samd21.I2cDriver.DmaChannel.READ]
      -> dmaDriver.abortTransactionIn[DmaChannel.SERCOM4_I2C_READ]
  dmaDriver.transactionIsrOut[DmaChannel.SERCOM4_I2C_READ]
      -> i2cDriver.dmaReplyIn[Samd21.I2cDriver.DmaChannel.READ]

  # Device <-> driver
  testSensor.i2cWriteRead     -> i2cDriver.writeRead
  i2cDriver.writeReadComplete -> testSensor.writeReadComplete
}
```

**Wiring requirements:**
- The WRITE and READ DMA ports must map to **two distinct physical DMA channels**.
- `reportTelemetryIn` should be connected to a rate group so bus-state and error
  telemetry is emitted.

## 5. Tested Configurations

| Board Name               | Chip       | SERCOM  | Pins        | Speed   | Device  | Ops tested       | Result |
| ------------------------ | ---------- | ------- | ----------- | ------- | ------- | ---------------- | ------ |
| Microchip Curiosity Nano | SAMD21G17A | SERCOM4 | PA12 / PA13 | 400 kHz | LTC2945 | write, writeRead | Pass   |

Verified end-to-end by reading LTC2945 registers over `writeRead` and confirming
the decoded ADC values are self-consistent and track the device's min/max hold
registers, demonstrating live repeated-START reads.

## 6. Limitations

- One transaction in flight at a time; concurrent requests are rejected with
  `I2C_OTHER_ERR`.
- Up to 255 bytes per transaction.
- 7-bit addressing only; High-speed master-code arbitration is not implemented.
- Completion callbacks run in ISR context; consumers must keep them short.
- Configuration is one-time; no runtime reconfiguration.

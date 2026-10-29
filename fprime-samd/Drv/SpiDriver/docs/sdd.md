# Samd21::SpiDriver

Driver for the SAMD21 SERCOM SPI peripheral in host mode, with DMA data movement and
asynchronous completion.

## 1. Introduction

`Samd21::SpiDriver` implements the F Prime `Drv.AsyncGuardedSpi` interface:
`Drv.AsyncSpiCfg.SpiDriverPorts` request ports (`SpiWriteRead`, one per attached device /
chip select) with matching `SpiWriteReadComplete` completion ports, delivered in main
context from `activeIn`. A request clocks one full-duplex frame — the write and
read buffers must be the same length — using two `Samd21.DmaDriver` channels (MISO
armed first, then MOSI, which starts the clock). The completion is reported when
both DMA channels have finished.

Chip select is either driven by the SERCOM (`HardwareChipSelect::ENABLED`, pad from
`DataOutPinout`) or by software through `chipSelectGpioOut[port]` around each frame.

The register-level code lives behind a tiny HAL (`SpiDriverHardware.hpp`):
`SpiDriverHardware.cpp` for the SAMD21 target, `SpiDriverHardwareStub.cpp` (which
records the configuration it was given) for native builds and the unit test. CMake
selects on `FPRIME_PLATFORM`.

## 2. Requirements

| Name | Description | Validation |
| ---- | ----------- | ---------- |
| SPI-CFG-001 | `configure()` shall program the SERCOM for SPI host mode once with the requested data order, clock polarity/phase, pad routing, run-in-standby and chip-select mode, and mark the driver configured. | Unit Test, Hardware Test |
| SPI-CFG-002 | With software chip select, `configure()` shall deassert (drive HIGH) every connected chip-select GPIO before the peripheral is enabled. | Unit Test |
| SPI-CFG-003 | With hardware chip select, the driver shall never drive `chipSelectGpioOut`. | Unit Test |
| SPI-CFG-004 | Reconfiguring an already configured driver shall assert. | Unit Test |
| SPI-CFG-005 | The BAUD register shall be computed as `fref / (2 * fsck) - 1`, rounded so the real SCK never exceeds the requested rate. | Unit Test |
| SPI-CFG-006 | `calculateBaud` shall assert on a zero rate, a rate above `fref / 2`, or a rate too slow for the 8-bit register, before any arithmetic that could overflow. | Unit Test |
| SPI-CFG-007 | With software chip select, `configure()` shall assert if a connected chip-select GPIO cannot be driven (`GpioStatus` other than `OP_OK`): the pin was not opened as an output before the driver was configured, an integration error. | Unit Test |
| SPI-CFG-008 | The SCK rate shall be changeable at runtime through the `setBaudRate` port (and the `setBaudRate()` method behind it): `SPI_CONFIG_ERR` before `configure()`, `SPI_BUSY_ERR` while a transfer is in flight, otherwise `SPI_OK`; the rate is left unchanged in both error cases and none of these paths asserts. A completed transfer whose reply `activeIn` has not delivered yet does not count as in flight. | Unit Test |
| SPI-CFG-009 | A baud change shall leave the SERCOM enabled: it disables the peripheral, rewrites the enable-protected `BAUD` register and re-enables it, waiting for `SYNCBUSY.ENABLE` each way, so that the next transfer completes normally. | Unit Test |
| SPI-REQ-001 | A request issued before `configure()` shall be refused with `SPI_CONFIG_ERR`, delivered from `activeIn`, and generate no DMA or chip-select traffic. | Unit Test |
| SPI-REQ-002 | A request whose write and read buffers differ in size shall assert (full duplex). | Unit Test |
| SPI-XFER-001 | A request shall assert the port's chip select (software mode) and queue the MISO then the MOSI DMA transaction against the SERCOM DATA register. | Unit Test, Hardware Test |
| SPI-XFER-002 | The reply shall be emitted on the requesting port once both DMA channels complete, in either order, after the chip select is deasserted. | Unit Test, Hardware Test |
| SPI-XFER-003 | One transaction at a time: an overlapping request on another slot shall be refused with `SPI_BUSY_ERR`, delivered from `activeIn`, and generate no DMA or chip-select traffic. A request on a slot that already holds an undelivered outcome (its transfer in flight, or its last completion not yet delivered) shall be refused with `SPI_BUSY_ERR` from `activeIn`, delivered before that outcome, without asserting; a third request on the same slot before `activeIn` runs is a client protocol error and asserts in the caller's context. | Unit Test |
| SPI-XFER-004 | With hardware chip select the transaction shall not drive the GPIO ports. | Unit Test |
| SPI-XFER-005 | A DMA completion with nothing in flight, a duplicate completion of one channel, or a completion on an unknown channel shall be ignored. | Unit Test |
| SPI-XFER-006 | If the GPIO driver refuses to assert the software chip select of a request, the driver shall not start the DMA streams, shall release the bus and shall refuse the request with `SPI_OTHER_ERR` from `activeIn`. | Unit Test |
| SPI-ERR-001 | `configure()` shall register for the SERCOM interrupt and enable only its `ERROR` source (receive buffer overflow, `STATUS.BUFOVF`), at the same NVIC priority as the DMAC interrupt. | Unit Test, Hardware Test |
| SPI-ERR-002 | On a receive overrun during a transaction the driver shall abort every DMA stream still outstanding via `dmaTransactionAbortOut`, acknowledge the hardware (wait for the shifter to go idle, clear `BUFOVF`, drain `DATA`, clear `INTFLAG.ERROR`), deassert the chip select (software mode) and reply on the requesting port with `SPI_OTHER_ERR` and the caller's buffers. | Unit Test, Hardware Test |
| SPI-ERR-003 | After an overrun reply the driver shall accept a new transaction immediately. | Unit Test, Hardware Test |
| SPI-ERR-004 | A SERCOM interrupt without `INTFLAG.ERROR`, or an overrun with no transaction in flight, shall not abort, reply or alter the transaction state (the latter is acknowledged). | Unit Test |
| SPI-ERR-005 | A DMA completion reporting `BUS_ERROR` on either stream shall abort the other stream if still outstanding, deassert the chip select (software mode) and reply on the requesting port with `SPI_OTHER_ERR` and the caller's buffers; the driver shall accept a new transaction immediately afterwards. | Unit Test |

## 3. Design

### 3.1 Ports

| Kind | Name | Type | Notes |
| ---- | ---- | ---- | ----- |
| guarded input | `SpiWriteRead[Drv.AsyncSpiCfg.SpiDriverPorts]` | `Drv.SpiWriteReadRequest` | from `Drv.AsyncGuardedSpi`; index = chip-select slot |
| output | `SpiWriteReadComplete[Drv.AsyncSpiCfg.SpiDriverPorts]` | `Drv.SpiWriteReadCallback` | **main context**, from `activeIn`; exactly one per request |
| sync input | `activeIn` | `Svc.ActiveSched` | **required**: delivers latched completions and refusals; returns false |
| sync input | `setBaudRate` | `Samd21.SpiBaudRate` | **main context**, same caller as `SpiWriteRead`; runtime SCK change, returns `Drv.SpiStatus` (§3.2) |
| output | `dmaTransactionOut[DmaChannel.N]` | `Dma.Transaction` | `MOSI`, `MISO` |
| output | `dmaTransactionAbortOut[DmaChannel.N]` | `Fw.Signal` | abort a stream after a receive overrun or a DMA bus error; **ISR context** |
| sync input | `dmaReplyIn[DmaChannel.N]` | `Dma.TransactionReply` | **ISR context**: latches the outcome; on `BUS_ERROR` aborts the other stream, acknowledges the SERCOM and releases the chip select (§3.4) |
| output | `chipSelectGpioOut[Drv.AsyncSpiCfg.SpiDriverPorts]` | `Drv.GpioWrite` | software chip select only |

`Drv.AsyncSpiCfg.SpiDriverPorts` (F Prime default 10) should be overridden in the project
configuration to the number of chip selects wired: each slot costs two pending-reply records
(outcome and refusal; plus three port objects -- request, completion, chip select -- in builds
without `FW_DIRECT_PORT_CALLS`; `fprime-samd-reference` uses 4). The `setBaudRate` port is one
object per driver, not per slot, and is optional: leave it unconnected if no client changes the
rate.

### 3.2 Configuration (`configure()`, `setBaudRate()`)

`configure(sercom, baud_rate_khz, DataOrder, ClockPolarity, ClockPhase, DataInPinout,
DataOutPinout, RunInStandby, HardwareChipSelect)` — called once from a `configComponents`
phase. The caller routes the pads (`Samd21::PinMux::configure`) and configures the
chip-select GPIO as an output **before** this call, because `configure()` deasserts
every connected software chip select. The GCLK core clock (`F_CPU`) is the baud reference.
`setBaudRate(baud_rate_khz)` -- the `setBaudRate` port, or the C++ method of the same name
that implements it -- changes SCK afterwards (SD cards: slow identification, fast data, and
again after a bus manager power-cycles the card). It returns `SPI_CONFIG_ERR` before
`configure()` and `SPI_BUSY_ERR` while a transfer is in flight, both without touching the
hardware; a transfer that has finished but whose reply `activeIn` has not delivered yet is not
in flight. Otherwise the HAL disables the SERCOM (`CTRLA.ENABLE = 0`, wait `SYNCBUSY.ENABLE`),
rewrites the enable-protected `BAUD` register, re-enables (`CTRLA.ENABLE = 1`, wait
`SYNCBUSY.ENABLE`) and `SPI_OK` is returned. Disabling does not reset any other register
(only `CTRLA.SWRST` does, DS40001882 §27.6.2.2), so mode, pinout, `INTENSET` and the DMA
trigger survive the cycle. The only assertion on this path is `calculateBaud` (SPI-CFG-006):
a rate the clock cannot produce is an integration error, not a runtime condition. The HAL
bounds each `SYNCBUSY` wait and asserts if the bit never clears (clock not running).

### 3.3 Transaction behaviour

```
SpiWriteRead[p] ─▶ unconfigured?          ─▶ latch (p, SPI_CONFIG_ERR)
                   slot p busy/undelivered? ─▶ refusal (p, SPI_BUSY_ERR)   no DMA, no chip select
                   bus busy (other slot)?   ─▶ latch (p, SPI_BUSY_ERR)     no DMA, no chip select
                   CS[p] LOW refused (sw)?  ─▶ bus released, latch (p, SPI_OTHER_ERR)   no DMA
                   else: CS[p] LOW (sw) → DMA MISO (DATA → read) → DMA MOSI (write → DATA)
   (ISR) dmaReplyIn[MISO], dmaReplyIn[MOSI] in either order; both done → CS[p] HIGH → latch (p, SPI_OK)
   (ISR) dmaReplyIn[*] BUS_ERROR → abort the other stream → drain/ack SERCOM → CS[p] HIGH
                                 → latch (p, SPI_OTHER_ERR)
   (ISR) SERCOM ERROR/BUFOVF → abort outstanding stream(s) → drain/ack SERCOM → CS[p] HIGH
                             → latch (p, SPI_OTHER_ERR)
activeIn ─▶ for every slot, refusal then latched outcome (each copied and cleared under
           CriticalSection): SpiWriteReadComplete[p](write, read, status)   main context
```

Nothing is delivered from an ISR: the ISR paths and the refusals only write the slot's
pending record; `activeIn` (one pass per `Svc.PassiveCycler` turn) emits the callbacks with
interrupts enabled, so a client may start its next transfer from inside the callback. A slot
holds one undelivered outcome (counting the transfer in flight) plus one refusal. A request
on a slot that already holds an outcome — its transfer is still in flight, or its last
completion has not been delivered yet — is latched in the slot's refusal record as
`SPI_BUSY_ERR` and delivered by `activeIn` ahead of the outcome; it is never asserted on,
because the trigger is a client re-request racing a completion, and nothing is called back
from inside the request. A third request on the same slot before `activeIn` has run finds
both records occupied and asserts (client protocol error, caller context). Stray DMA replies
(nothing in flight, duplicate channel, unknown channel) are ignored — the driver never asserts
in response to bus traffic.

### 3.4 Error handling

**Receive overrun.** The SERCOM receive buffer is two characters deep. If the MISO DMA
channel loses arbitration for longer than two character times (observed on hardware at
12 MHz SCK with the UART and I2C channels on the same DMAC priority level), the SERCOM
drops a character and sets `STATUS.BUFOVF`/`INTFLAG.ERROR`. The MISO DMA block can then
never reach its count and, without intervention, the transaction never completes.
`configure()` therefore enables the SERCOM `ERROR` interrupt; `isrHandler()` aborts the
outstanding stream(s) through `dmaTransactionAbortOut` (the DMA driver clears the
channel's pending flags and raises no reply), acknowledges the overflow (bounded wait for
`TXC` so the character still in the shifter cannot land after the drain, write-1-clear
`BUFOVF`, drain `DATA`, clear `TXC`/`ERROR`), deasserts the software chip select and latches
`SPI_OTHER_ERR` with the caller's buffers for the next `activeIn`. The SERCOM and DMAC
interrupts share the lowest NVIC priority, so neither handler preempts the other inside the
driver.

Nothing in the stack detects a transfer that produces no clock at all (e.g. SERCOM clock
gated); that failure mode has not been observed and a tick-based timeout is left for §6.

**DMA bus error.** A `dmaReplyIn` carrying `BUS_ERROR` (the DMAC faulted on a caller
buffer) aborts the other stream if it is still outstanding, drains and acknowledges the
SERCOM receiver exactly as after an overrun, deasserts the chip select and latches
`SPI_OTHER_ERR` for the requesting slot; the next request is accepted immediately. Caller
programming errors (size mismatch, > 65535 bytes, reconfigure, a third request on one slot
within a single `activeIn` turn) assert in the caller's context; `setBaudRate` while busy or
before `configure()` is reported through its return status instead (§3.2).

### 3.5 Tested configurations

| Board / device | SERCOM, pads | SCK | Mode | Chip select | Result |
| --- | --- | --- | --- | --- | --- |
| SAMD21 Curiosity Nano + Infineon CY15B102QN F-RAM | SERCOM1, MOSI PA16 / SCK PA17 / MISO PA19 | 1 MHz | 0, 3 | software (PA18) | pass |
| same | same | 4 MHz | 0 | software | pass (qualification speed of `Fram.FramDriver`) |
| same | same | 12 MHz | 0 | software | functional; receive overruns under DMA contention, every one recovered (33 in 150 s) |

Limitations: hardware chip select, modes 1 and 2, LSB-first order, 9-bit characters and
SERCOMs other than 1 are implemented from the data sheet but not exercised on hardware; one
transfer at a time per driver (no queueing across slots); ≤ 65535 bytes per transfer.

## 4. Integration

A deployment supplies two DMA channels (`dmaTransactionOut`/`dmaReplyIn`/`dmaTransactionAbortOut`
per channel), optionally a `Samd21.GpioDriver` chip select, `cycler.cycleOut -> spiDriver.activeIn`
before the consumers of its completions, and calls `configure()` in the `configComponents` phase.
The CY15B102QN F-RAM driver (separate PR) is the reference consumer.

## 5. Unit tests

`test/ut/` runs natively against the stub HAL (21 tests): configuration in both chip-select
modes, reconfigure assert, request before configure (`SPI_CONFIG_ERR` via `activeIn`), nominal
transactions in both DMA completion orders on every slot, busy refusal on another slot
(`SPI_BUSY_ERR`, delivered by `activeIn`), re-request on the in-flight slot and on a slot with
an undelivered completion (`SPI_BUSY_ERR` latched and delivered ahead of the outcome, third
request asserts), DMA `BUS_ERROR` on either stream and on the second completion (other stream
aborted, chip select released, `SPI_OTHER_ERR`, bus reusable), hardware chip
select, chip select refused by the GPIO driver (`SPI_OTHER_ERR`, no DMA traffic; assert in
`configure()`), size mismatch, stray/duplicate/late DMA replies ignored, `activeIn` with nothing
pending, `setBaudRate` method and port (idle, before configure, while busy, with a latched
undelivered reply; the stub HAL models the SERCOM enable state and the test proves a full
transfer completes with `SPI_OK` after the change), BAUD boundaries and overflow,
500 randomized transactions, and the receive-overrun path (both streams outstanding, one
stream outstanding, spurious interrupt, overrun while idle, recovery on every slot and in
both chip-select modes) driven through the stub HAL's `fireIsr()`.

## 6. Future work

- A tick-based completion timeout for faults that raise no SERCOM flag (none observed).
- Raise the MISO channel above the MOSI/UART channels in DMAC priority so the overrun
  does not occur in the first place at high SCK rates.

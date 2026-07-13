# Samd21::UsartDriver

## 1. Introduction

The `Samd21::UsartDriver` component provides a hardware driver for the SAMD21 SERCOM peripheral configured in USART (Universal Synchronous and Asynchronous Receiver Transmitter) mode. This passive component uses a custom `AsyncNonQueuedByteStreamDriver` interface inspired by F´'s `Drv.ByteStreamDriver` pattern but adapted for baremetal deployments with DMA-accelerated transmission and reception.

The driver uses a dual-buffer circular receive strategy with DMA channels for both TX and RX operations. Transmission is queue-based with asynchronous completion callbacks. On receive, both RX buffers are queued into the DMA circular chain during configuration and remain there for the lifetime of the driver — they are never popped off the chain. Instead, the driver extracts data in-place from the buffer the DMA is actively filling, tracking a per-buffer offset so that both full buffers (DMA completion) and partial buffers (in-progress receive) can be drained downstream without disturbing the DMA. The component is cycled via the `activeIn` port (typically connected to a `Svc::PassiveCycler` component) to process DMA completion signals and the `schedIn` port (typically connected to a rate group) to poll the in-progress RX transfer for newly received bytes on every tick.

**Design Note:** This driver does NOT implement the standard F´ `Drv.ByteStreamDriver` interface. The custom `AsyncNonQueuedByteStreamDriver` interface is required because the standard interface assumes either an active component with a queue or a purely synchronous passive component. This driver needs neither — it's passive (no thread) but handles asynchronous DMA completions via ISR-to-main-loop signaling. The topology must use baremetal-aware components (e.g., `Samd21.Framer`) that understand this custom interface rather than standard F´ components expecting `Drv.ByteStreamDriver`.

## 2. Requirements

| Name            | Description                                                                                                                                                                                                                                                                                   | Validation    |
| --------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------- |
| SAMD21-UART-001 | The UsartDriver shall configure SERCOM peripherals for USART operation with configurable baud rates (9600-921600), data formats (5-9 bits), parity (none/even/odd), stop bits (1 or 2), bit ordering (MSB/LSB-first), communication modes (async/sync), and clock sources (internal/external) | Hardware Test |
| SAMD21-UART-002 | The UsartDriver shall use DMA for both transmission and reception                                                                                                                                                                                                                             | Hardware Test |
| SAMD21-UART-003 | The UsartDriver shall implement double-buffering for continuous RX                                                                                                                                                                                                                            | Hardware Test |
| SAMD21-UART-004 | The UsartDriver shall poll the in-progress RX transfer on each rate group tick to extract partial RX frames                                                                                                                                                                                   | Hardware Test |
| SAMD21-UART-005 | The UsartDriver shall queue TX requests with asynchronous completion                                                                                                                                                                                                                          | Hardware Test |
| SAMD21-UART-006 | The UsartDriver shall provide synchronous blocking transmission for early boot diagnostics                                                                                                                                                                                                    | Hardware Test |

## 3. Design

### 3.1 Component Diagram

`Samd21::UsartDriver` is a passive component that uses a custom `AsyncNonQueuedByteStreamDriver` interface defined in the same FPP file. This interface is based on the F´ `Drv.ByteStreamDriver` pattern but is specifically designed for baremetal deployments using passive cycling and DMA. Unlike the standard `Drv.ByteStreamDriver`, this component requires explicit wiring to a `PassiveCycler` (via `activeIn`) to function, and is recommended to be wired to a rate group (via `schedIn`) to extract partial RX frames.

### 3.2 Ports

| Kind         | Name             | Port Type              | Usage                                                                                                     |
| ------------ | ---------------- | ---------------------- | --------------------------------------------------------------------------------------------------------- |
| `sync input` | `send`           | `Fw.BufferSend`        | Send data buffer out the UART (queued for DMA transmission)                                               |
| `sync input` | `sendSync`       | `Drv.ByteStreamSend`   | Synchronously transmit data (blocks until complete, no DMA)                                               |
| `sync input` | `recvReturnIn`   | `Fw.BufferSend`        | Return ownership of a received buffer view (no-op; buffers stay in the DMA chain)                         |
| `sync input` | `dmaReplyIn[2]`  | `Dma.TransactionReply` | DMA completion signals from DMAC (called in ISR context)                                                  |
| `sync input` | `schedIn`        | `Svc.Sched`            | **RECOMMENDED:** Rate group handler that polls the in-progress RX transfer (should connect to rate group) |
| `sync input` | `activeIn`       | `Svc.Sched`            | **REQUIRED:** Signal queue processor (must connect to PassiveCycler)                                      |
| `output`     | `ready`          | `Drv.ByteStreamReady`  | Signals driver is ready to send/receive data                                                              |
| `output`     | `recv`           | `Drv.ByteStreamData`   | Outputs received data to downstream consumers                                                             |
| `output`     | `sendReturnOut`  | `Drv.ByteStreamData`   | Returns ownership of sent buffer with completion status                                                   |
| `output`     | `dmaQueueOut[2]` | `Dma.Transaction`      | Sends DMA transaction requests to DMAC driver                                                             |
| `output`     | `dmaRxCircular`  | `Fw.Signal`            | Configures RX DMA channel for circular mode                                                               |
| `output`     | `dmaRxRead`      | `Dma.ReadWriteback`    | Reads the active RX transfer's remaining byte count in place (no descriptor pop)                          |

#### 3.2.1 activeIn and schedIn Port Pattern

The driver uses two cyclic inputs — `activeIn` is required for the driver to function; `schedIn` is recommended for partial RX frame extraction:

**activeIn (PassiveCycler) — required:**
- Processes the internal signal queue populated by DMA ISR handlers
- Dequeues TX/RX completion signals and invokes output ports
- Returns buffers to senders/consumers with status
- Must be driven by `Svc::PassiveCycler` from the main loop
- Without this connection, all TX/RX operations will hang

**schedIn (Rate Group) — recommended:**
- Polls the in-progress RX transfer on every tick via `dmaRxRead_out()`
- Enqueues a `RX_BUFFER_PARTIAL` signal whenever new bytes have arrived since the last extraction
- The rate group frequency directly sets the partial-frame extraction latency and granularity
- Typically connected to a 1Hz-8Hz rate group
- Without this connection, driver can only receive full buffers

This pattern separates interrupt handling (minimal ISR that sets a flag) from signal processing (full F´ port invocations), keeping ISR latency low while maintaining clean component boundaries.

#### 3.2.2 Why Sync Ports?

The `send` and `recvReturnIn` ports are `sync` (not `guarded`) because this is a baremetal deployment with no RTOS — there are no concurrent threads, only ISR context and main context. All port invocations happen in the main context (via `activeIn_handler` or external callers). The internal signal queue already provides ISR-to-main-context decoupling. Mutex protection (`guarded`) would add unnecessary overhead with no concurrency benefit.

### 3.3 Hardware Configuration

The driver configures SAMD21 SERCOM peripherals (SERCOM0-5, device-dependent) with the following features:

| Feature           | Configuration                                                       |
| ----------------- | ------------------------------------------------------------------- |
| Clock Source      | Internal (GCLK0 @ 48MHz) or External                                |
| Communication     | Asynchronous (standard UART) or Synchronous                         |
| Baud Rate         | 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600           |
| Data Bits         | 5, 6, 7, 8, or 9 bits                                               |
| Parity            | None, Even, or Odd                                                  |
| Stop Bits         | 1 or 2                                                              |
| Bit Order         | MSB-first (non-standard) or LSB-first (standard)                    |
| Pin Configuration | Configurable RX (PAD0-3) and TX (PAD0/2 with optional flow control) |
| DMA Channels      | 2 channels (one for TX, one for RX)                                 |
| RX Buffer         | Double-buffered circular mode for continuous reception              |

### 3.4 Public Interface

```cpp
enum class ClockMode : U8 {
    EXTERNAL = 0x0,  // External clock on XCK pin
    INTERNAL = 0x1,  // Internal clock (48MHz GCLK0)
};

enum class CommunicationMode : U8 {
    ASYNC = 0x0,  // Asynchronous mode (standard UART)
    SYNC = 0x1,   // Synchronous mode (clock on XCK pin)
};

enum class RxPinOut : U8 {
    PAD0 = 0, PAD1 = 1, PAD2 = 2, PAD3 = 3,
};

enum class TxPinOut : U8 {
    PAD0 = 0,                    // TX on PAD[0], XCK on PAD[1]
    PAD2_XCK_PAD3 = 1,           // TX on PAD[2], XCK on PAD[3]
    PAD0_RTS_PAD2_CTS_PAD3 = 2,  // TX on PAD[0], RTS on PAD[2], CTS on PAD[3]
};

enum class DataOrder : U8 {
    MSB_FIRST = 0x0,  // MSB transmitted first (non-standard)
    LSB_FIRST = 0x1,  // LSB transmitted first (standard)
};

enum class Parity : U8 {
    NONE, EVEN, ODD,
};

enum class StopBits : U8 {
    ONE = 0x0, TWO = 0x1,
};

enum class DataBits : U8 {
    BITS_8 = 0x0, BITS_9 = 0x1,
    BITS_5 = 0x5, BITS_6 = 0x6, BITS_7 = 0x7,
};

enum class BaudRate : U32 {
    BAUD_9600 = 9600,     BAUD_19200 = 19200,
    BAUD_38400 = 38400,   BAUD_57600 = 57600,
    BAUD_115200 = 115200, BAUD_230400 = 230400,
    BAUD_460800 = 460800, BAUD_921600 = 921600,
};

void configure(SercomKind sercom,
               RxPinOut rx,
               TxPinOut tx,
               ClockMode clock,
               CommunicationMode mode,
               BaudRate baud_rate,
               DataOrder data_order,
               DataBits data_bits,
               StopBits stop_bits,
               Parity parity);
```

### 3.5 Transmission Architecture

The driver implements two transmission modes:

**Asynchronous Transmission (DMA-based):**
- FIFO queue holds up to `USART_TX_BUFFER_N` pending TX buffers (default: 2)
- When `send_handler()` is called, buffer is enqueued and DMA transaction is submitted
- If queue is full, `SEND_RETRY` status is returned immediately
- DMA completion triggers ISR which enqueues a signal for deferred processing
- `activeIn_handler()` dequeues signals and calls `sendReturnOut_out()` with final status

**Synchronous Transmission (Blocking):**
- Invoked via `sendSync_handler()` for early boot diagnostics
- Blocks the calling context until all bytes are transmitted
- Polls USART DATA register directly without DMA
- Returns immediately with `OP_OK` status (no async callback)
- Not recommended for production telemetry (use async `send` instead)

### 3.6 Reception Architecture

The driver implements a double-buffered circular receive strategy with per-tick polling. The two RX buffers are queued into the DMA circular chain once during `configure()` and are **never popped off the chain**. Rather than handing DMA descriptors to downstream consumers, the driver reads data out of the buffer the DMA is currently filling and forwards views into it, so the DMA continues receiving uninterrupted.

**RX Double-Buffer Operation:**
- Two equal-size buffers (A and B) statically allocated within the component
- Both buffers queued to DMA in circular mode during `configure()` and left in the chain permanently
- `m_active_rx` tracks which buffer (A or B) the DMA is currently filling
- `m_active_processed` tracks how many bytes of the active buffer have already been forwarded downstream
- When the DMA fills a buffer completely (`RX_BUFFER_DONE`), the remainder of that buffer (from `m_active_processed` to the end) is forwarded downstream, then `m_active_rx` flips to the other buffer and `m_active_processed` resets to 0. The DMA has already advanced to the other buffer via the circular chain.

**Data Extraction (In-Place):**
- The count of newly available bytes is computed as `USART_RX_BUFFER_SIZE - btcnt - m_active_processed`, where `btcnt` is the DMA's remaining beat count
- This count is computed at signal-generation time (in `schedIn_handler()` or the RX ISR) and carried in the signal's `rx_bytes` field, so `activeIn_handler()` uses it directly
- A `Fw::Buffer` view is constructed pointing at `m_rx[active].data + m_active_processed` with length `rx_bytes`
- The buffer view is forwarded downstream via `recv_out()` only if it is non-empty

**Per-Tick RX Polling:**
- On each `schedIn_handler()` call (rate group tick), the DMA's remaining beat count is read via `dmaRxRead_out()` without modifying the descriptor
- If new bytes have arrived since the last extraction (`USART_RX_BUFFER_SIZE - btcnt - m_active_processed > 0`), a `RX_BUFFER_PARTIAL` signal carrying that byte count is enqueued for processing in `activeIn_handler()`; if no new bytes have arrived, no signal is enqueued
- On a partial event, the newly received bytes are extracted in place and forwarded, and `m_active_processed` is advanced by that amount so the same bytes are not sent twice; the active buffer and DMA state are left untouched
- There is no watchdog counter — every tick polls the DMA directly, so extraction latency is bounded by a single rate group period

**Buffer Return:**
- Because RX buffers are never removed from the DMA chain, `recvReturnIn_handler()` is a no-op — downstream consumers borrow a view into a live DMA buffer and simply return ownership when done

This mechanism allows extraction of short messages without waiting for a full buffer, while keeping both RX buffers continuously available to the DMA.

### 3.7 Signal Queue and ISR Safety

The driver uses an internal signal queue to decouple ISR context from port invocations:

**Signal Queue:**
- DMA completion handlers run in ISR context
- ISR enqueues signals to internal queue (max 4 entries)
- `activeIn_handler()` runs in normal context (via PassiveCycler)
- Handler dequeues all pending signals and invokes output ports
- Ensures no F´ port calls occur within ISR context

**Signal Types:**
- `TX_BUFFER_OK`: TX DMA completed successfully
- `TX_CHANNEL_ERROR`: TX DMA bus error (clears entire TX queue)
- `RX_BUFFER_PARTIAL`: A `schedIn` poll found new bytes; drain them from the active buffer in place (advances `m_active_processed`, leaves the active buffer in the DMA chain)
- `RX_BUFFER_DONE`: RX buffer filled; drain the buffer remainder, then flip the active buffer and reset `m_active_processed`
- `RX_CHANNEL_ERROR`: RX DMA bus error (currently unhandled, asserts)

Both RX signals carry the count of newly available bytes in the `rx_bytes` field, computed at signal-generation time; `activeIn_handler()` uses it directly to size the forwarded buffer view.

## 4. Usage

### 4.1 Configuration

The driver supports compile-time configuration via [`fprime-samd/config/UsartDriverConfig.hpp`](../../config/UsartDriverConfig.hpp):

```cpp
namespace Samd21 {
enum UsartDriverConfig {
    //! Length of the TX job queue
    USART_TX_BUFFER_N = 2,
    
    //! Length of each Rx buffer
    //! Each USART driver will allocate two Rx buffers
    USART_RX_BUFFER_SIZE = 256,
};
}
```

**Configuration Notes:**
- `USART_TX_BUFFER_N`: Increasing this allows more TX requests to be queued simultaneously, reducing `SEND_RETRY` occurrences under high TX load. Default is 2.
- `USART_RX_BUFFER_SIZE`: Each UsartDriver instance allocates **two** RX buffers of this size statically. Total memory per instance = `2 * USART_RX_BUFFER_SIZE` bytes. On SAMD21 with 16KB RAM, this is a significant allocation — size accordingly. Default is 256 bytes (512 bytes total per instance).

### 4.2 Error Handling

**TX Channel Errors:**
When a DMA bus error occurs on the TX channel, the driver:
1. Dequeues all pending TX buffers from the TX queue
2. Returns each buffer to the sender via `sendReturnOut_out()` with status `OTHER_ERROR`
3. This allows the sender to retry transmission after the DMA channel recovers

**RX Channel Errors:**
RX DMA bus errors currently trigger an assertion (unhandled error condition). Future implementations may re-queue the RX buffer to DMA or reset the RX DMA channel.

### 4.3 Initialization

**IMPORTANT:** GPIO pin muxing must be configured BEFORE calling `configure()`. Use `Samd21::PinMux::configure()` to assign GPIO pins to the SERCOM peripheral function.

#### 4.3.1 Standard Async UART (115200 8N1, LSB-first)

```cpp
// Configure GPIO pins for SERCOM0
Samd21::PinMux::configure(PINMUX_PA22C_SERCOM0_PAD0);  // PA22 -> SERCOM0 PAD[0] (TX)
Samd21::PinMux::configure(PINMUX_PA23C_SERCOM0_PAD3);  // PA23 -> SERCOM0 PAD[3] (RX)

usartDriver.configure(
    SercomKind::SERCOM_0,
    UsartDriver::RxPinOut::PAD3,    // RX on PAD3
    UsartDriver::TxPinOut::PAD0,    // TX on PAD0
    UsartDriver::ClockMode::INTERNAL,
    UsartDriver::CommunicationMode::ASYNC,
    UsartDriver::BaudRate::BAUD_115200,
    UsartDriver::DataOrder::LSB_FIRST,  // Standard
    UsartDriver::DataBits::BITS_8,
    UsartDriver::StopBits::ONE,
    UsartDriver::Parity::NONE
);
```

**Note on Data Order:**
- `DataOrder::LSB_FIRST` is the **standard** UART bit order used by most devices and should be used for normal operation
- `DataOrder::MSB_FIRST` is **non-standard** and only needed for compatibility with specific hardware that expects MSB-first transmission

**Configuration Steps:**
The `configure()` method performs the following:
1. Validates configuration parameters (called only once)
2. Enables SERCOM peripheral clock and assigns GCLK0 (48MHz)
3. Builds and writes CTRLA/CTRLB registers (mode, pins, parity, stop bits, data bits)
4. Calculates and writes BAUD register (for internal clock mode)
5. Enables USART peripheral, transmitter, and receiver
6. Initializes RX state machine and queues both RX buffers to DMA
7. Configures RX DMA for circular mode
8. Signals ready via `ready_out()` port

**Safety Features:**
- All register writes wait for SYNCBUSY to clear
- Configuration can only be called once (asserted)
- SERCOM hardware pointer validated

### 4.4 Driving the Component

The UsartDriver requires two cyclic inputs:

**Active Cycling (PassiveCycler):**

```cpp
// Main loop
while (true) {
    passiveCycler.cycle();  // Calls usartDriver.activeIn_handler()
    __WFI();                // Optional: sleep until interrupt
}
```

The `activeIn_handler()` processes the signal queue, returning buffers and status to producers/consumers.

**Rate Group Cycling:**

```cpp
// Topology connections
rateGroup1Hz.RateGroupMemberOut[0] -> usartDriver.schedIn
```

The `schedIn_handler()` polls the in-progress RX transfer on every tick and enqueues a partial-frame extraction whenever new bytes have arrived.

### 4.5 Topology Connections

```fpp
instance passiveCycler: Svc.PassiveCycler base id 0xD00
instance usartDriver: Samd21.UsartDriver base id 0xF00
instance dmacDriver: Samd21.DmacDriver base id 0xF10
instance rateGroup1Hz: Svc.ActiveRateGroup base id 0xF30

connections UsartComm {
  # REQUIRED: Cycler drives the driver (processes signal queue from ISRs)
  passiveCycler.cycleOut -> usartDriver.activeIn

  # RECOMMENDED: Rate group ticks drive per-tick RX polling (detects partial RX frames)
  rateGroup1Hz.RateGroupMemberOut[0] -> usartDriver.schedIn

  # DMA channel connections (TX)
  usartDriver.dmaQueueOut[Samd21.UsartDriver.DmaChannel.TX] -> dmaDriver.sendTransactionIn[0]
  dmaDriver.transactionIsrOut[0] -> usartDriver.dmaReplyIn[Samd21.UsartDriver.DmaChannel.TX]

  # DMA channel connections (RX)
  usartDriver.dmaQueueOut[Samd21.UsartDriver.DmaChannel.RX] -> dmaDriver.sendTransactionIn[1]
  dmaDriver.transactionIsrOut[1] -> usartDriver.dmaReplyIn[Samd21.UsartDriver.DmaChannel.RX]
  usartDriver.dmaRxCircular -> dmaDriver.linkToFrontIn[1]
  usartDriver.dmaRxRead -> dmaDriver.readWritebackIn[1]
}
```

**Wiring Requirements:**
- `activeIn` **must** be connected to a `PassiveCycler` driven by the main loop. Without this connection, TX/RX buffers will never be returned to their owners.
- `schedIn` **should** be connected to a rate group (1Hz-8Hz recommended). Without this connection, the driver can only receive full buffers and will miss short messages.

### 4.6 Tested Configurations

This component has been tested on the following hardware venues with the following tests:

1. In the main F Prime communications pipeline. Here we do two things:
    a. Send telemetry and events at the requested baud-rate
    b. Receive command packets at the requested baud. The ground will spam NO_OP commands at the maximum rate of the UART bus and we will watch for dispatch.

    **This test is the `Fw` column**. It is the rate that we can practically run at before the MCU chokes on too many commands.

2. Loopback mode. Use a component that Tx-es what it Rx-es
    a. The ground will just spam NO_OP commands though it doesn't make a difference

    **This test is the `Loopback` column**

| Board Name               | Chip       | SERCOM  | Config     | Baud   | Data Order | Loopback | Fw  |
| ------------------------ | ---------- | ------- | ---------- | ------ | ---------- | -------- | --- |
| Microchip Curiosity Nano | SAMD21G17A | SERCOM0 | Async, 8N1 | 115200 | LSB-first  |          | Y   |
| Microchip Curiosity Nano | SAMD21G17A | SERCOM3 | Async, 8N1 | 115200 | LSB-first  | Y        |     |
| Microchip Curiosity Nano | SAMD21G17A | SERCOM3 | Async, 8N1 | 230400 | LSB-first  | Y        |     |
| Microchip Curiosity Nano | SAMD21G17A | SERCOM3 | Async, 8N1 | 460800 | LSB-first  | Y        |     |
| Microchip Curiosity Nano | SAMD21G17A | SERCOM3 | Async, 8N1 | 921600 | LSB-first  | Y        | N   |

## 5. Performance Characteristics

### 5.1 Timing

- **TX Latency**: Single DMA setup overhead (?? CPU cycles) + DMA transfer time
- **RX Latency**: DMA transfer time + up to one rate group period (partial frames extracted on the next `schedIn` tick)
- **ISR Duration**: < ?? CPU cycles (signal enqueue only, no port calls)

### 5.2 Memory Usage

- **Static per instance**: 
  - Component state: ~80 bytes (queues, state machines, flags)
  - RX buffers: `2 * USART_RX_BUFFER_SIZE` bytes (default: 512 bytes)
  - TX queue: `USART_TX_BUFFER_N * sizeof(ThinBuffer)` ≈ 32 bytes
  - **Total**: 660 bytes per instance (with default config)
- **Dynamic**: 0 bytes (no heap allocations)
- **Stack**: Minimal (< 100 bytes worst-case for handler call chains)

### 5.3 Limitations

- Maximum TX queue depth: `USART_TX_BUFFER_N` (configurable, default = 2)
- RX buffer overrun occurs if the driver is not cycled (`activeIn`/`schedIn`) fast enough to drain the active buffer before the DMA laps back around the circular chain and overwrites unread bytes. Because downstream consumers borrow a view into a live DMA buffer, they must consume the data before this happens.
- Partial RX frame extraction resolution limited by rate group frequency (e.g., 125ms at 8Hz)
- Component configuration is one-time only (no runtime reconfiguration)

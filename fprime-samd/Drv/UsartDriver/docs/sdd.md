# Samd21::UsartDriver

## 1. Introduction

The `Samd21::UsartDriver` component provides a hardware driver for the SAMD21 SERCOM peripheral configured in USART (
Universal Synchronous and Asynchronous Receiver Transmitter) mode. This passive component implements the F´
`Drv.ByteStreamDriver` interface with DMA-accelerated transmission and reception, supporting both asynchronous and
synchronous communication modes.

The driver uses a dual-buffer circular receive strategy with DMA channels for both TX and RX operations. Transmission is
queue-based with asynchronous completion callbacks, while reception uses double-buffering with an idle watchdog to
detect partial frames. The component is cycled via the `activeIn` port (typically connected to a `Svc::PassiveCycler`
component) to process DMA completion signals and the `schedIn` port (typically connected to a rate group) to detect idle
periods on receive.

## 2. Requirements

| Name            | Description                                                                                                                                                                                                                                                                                   | Validation    |
|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|---------------|
| SAMD21-UART-001 | The UsartDriver shall configure SERCOM peripherals for USART operation with configurable baud rates (9600-921600), data formats (5-9 bits), parity (none/even/odd), stop bits (1 or 2), bit ordering (MSB/LSB-first), communication modes (async/sync), and clock sources (internal/external) | Hardware Test |
| SAMD21-UART-002 | The UsartDriver shall use DMA for both transmission and reception                                                                                                                                                                                                                             | Hardware Test |
| SAMD21-UART-003 | The UsartDriver shall implement double-buffering for continuous RX                                                                                                                                                                                                                            | Hardware Test |
| SAMD21-UART-004 | The UsartDriver shall detect idle periods to extract partial RX frames                                                                                                                                                                                                                        | Hardware Test |
| SAMD21-UART-005 | The UsartDriver shall queue TX requests with asynchronous completion                                                                                                                                                                                                                          | Hardware Test |

## 3. Design

### 3.1 Component Diagram

`Samd21::UsartDriver` is a passive component that implements the `Drv.ByteStreamDriver` interface pattern using the
custom `AsyncNonQueuedByteStreamDriver` interface defined in the same FPP file.

### 3.2 Ports

| Kind            | Name              | Port Type              | Usage                                                    |
|-----------------|-------------------|------------------------|----------------------------------------------------------|
| `guarded input` | `send`            | `Fw.BufferSend`        | Send data buffer out the UART (queued asynchronously)    |
| `guarded input` | `recvReturnIn`    | `Fw.BufferSend`        | Return ownership of received buffer back to driver       |
| `sync input`    | `dmaReplyIn[2]`   | `Dma.TransactionReply` | DMA completion signals from DMAC (called in ISR context) |
| `sync input`    | `schedIn`         | `Svc.Sched`            | Rate group handler for idle detection watchdog           |
| `sync input`    | `activeIn`        | `Svc.Sched`            | Active handler for processing DMA completion queue       |
| `output`        | `ready`           | `Drv.ByteStreamReady`  | Signals driver is ready to send/receive data             |
| `output`        | `recv`            | `Drv.ByteStreamData`   | Outputs received data to downstream consumers            |
| `output`        | `sendReturnOut`   | `Drv.ByteStreamData`   | Returns ownership of sent buffer with status             |
| `output`        | `allocate`        | `Fw.BufferGet`         | Allocates RX buffers during initialization               |
| `output`        | `deallocate`      | `Fw.BufferSend`        | Deallocates buffers (unused currently)                   |
| `output`        | `dmaQueueOut[2]`  | `Dma.Transaction`      | Sends DMA transaction requests to DMAC driver            |
| `output`        | `dmaRxCircular`   | `Fw.Signal`            | Configures RX DMA channel for circular mode              |
| `output`        | `dmaRxPopCurrent` | `Dma.ReadWriteback`    | Reads current RX DMA transfer state during idle          |

### 3.3 Hardware Configuration

The driver configures SAMD21 SERCOM peripherals (SERCOM0-5, device-dependent) with the following features:

| Feature           | Configuration                                                       |
|-------------------|---------------------------------------------------------------------|
| Clock Source      | Internal (GCLK0 @ 48MHz) or External                                |
| Communication     | Asynchronous (standard UART) or Synchronous                         |
| Baud Rate         | 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600           |
| Data Bits         | 5, 6, 7, 8, or 9 bits                                               |
| Parity            | None, Even, or Odd                                                  |
| Stop Bits         | 1 or 2                                                              |
| Bit Order         | MSB-first or LSB-first                                              |
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
    PAD0 = 0,  // TX on PAD[0], XCK on PAD[1]
    PAD1 = 1,  // TX on PAD[2], XCK on PAD[3]
    PAD2 = 2,  // TX on PAD[0], RTS on PAD[2], CTS on PAD[3]
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
               Parity parity,
               FwSizeType rx_buffer_size,
               U16 rx_dog_cnt);
```

### 3.5 Transmission Architecture

The driver implements an asynchronous transmission queue:

**TX Queue Management:**

- FIFO queue holds up to `USART_TX_BUFFER_N` pending TX buffers (configurable via `UsartDriverConfig.hpp`, default = 2)
- When `send_handler()` is called:
    1. Buffer is enqueued to `m_tx_queue`
    2. DMA transaction is immediately submitted to DMAC driver
    3. If queue is full, `SEND_RETRY` status is returned immediately
- DMA completion triggers `dmaReplyIn_handler()` in ISR context
- ISR enqueues a signal to `m_queue` for deferred processing
- `activeIn_handler()` dequeues signals and calls `sendReturnOut_out()` with final status

**TX DMA Configuration:**

- Source: TX buffer data address (increment enabled)
- Destination: SERCOM `DATA` register (no increment)
- Beat size: 1 byte
- Trigger: SERCOM TX DRE (Data Register Empty) trigger
- Priority: PRIORITY_0
- Step size: 1 byte

### 3.6 Reception Architecture

The driver implements a double-buffered circular receive strategy with idle detection:

**RX Buffer States:**

- `UNINITIALIZED`: Buffer not yet allocated
- `DMA`: Buffer actively receiving data via DMA
- `DMA_WAITING`: Buffer queued to DMA, waiting to become active
- `IN_USE`: Buffer sent to downstream consumer, awaiting return

**RX Double-Buffer Operation:**

1. Two equal-size buffers (A and B) allocated via `allocate_out()` port
2. Both buffers queued to DMA in circular mode during `configure()`
3. At any time:
    - One buffer is actively receiving (state = `DMA`)
    - One buffer is waiting to become active (state = `DMA_WAITING`)
4. When a buffer fills:
    - DMA automatically switches to the waiting buffer (circular chain)
    - Filled buffer transitions to `IN_USE` state
    - Buffer sent downstream via `recv_out()`
5. When downstream returns buffer via `recvReturnIn_handler()`:
    - Buffer transitions to `DMA_WAITING` state
    - Already queued in circular chain, automatically used when active buffer fills

**Idle Detection Watchdog:**

- Watchdog counter (`m_rx_dog`) initialized to `rx_dog_cnt` at configuration
- Decremented on each `schedIn_handler()` call (rate group tick)
- When counter reaches zero:
    1. Current RX transfer state read via `dmaRxPopCurrent_out()`
    2. Partial buffer extracted with `btcnt` (bytes remaining) from DMA
    3. Signal enqueued: `RX_BUFFER_DONE` with remaining byte count
    4. `activeIn_handler()` processes signal:
        - Calculates received bytes: `buffer_size - btcnt`
        - If bytes > 0: sends partial buffer downstream via `recv_out()`
        - If bytes == 0: returns buffer to DMA_WAITING state
    5. Watchdog counter reset to `rx_dog_cnt`

This mechanism allows detection of short messages or idle periods without waiting for a full buffer.

**RX DMA Configuration:**

- Source: SERCOM `DATA` register (no increment)
- Destination: RX buffer data address (increment enabled)
- Beat size: 1 byte
- Trigger: SERCOM RX RXC (Receive Complete) trigger
- Priority: PRIORITY_0
- Step size: 1 byte
- Mode: Circular (via `dmaRxCircular_out()`)

### 3.7 Signal Queue and ISR Safety

The driver uses a signal queue (`m_queue`) to decouple ISR context from port invocations:

**Signal Types:**

- `TX_BUFFER_OK`: TX DMA completed successfully
- `TX_CHANNEL_ERROR`: TX DMA bus error (clears entire TX queue)
- `RX_BUFFER_DONE`: RX buffer filled or partial frame detected
- `RX_CHANNEL_ERROR`: RX DMA bus error

**Processing Flow:**

1. DMA completion handlers (`dmaReplyTxIsr`, `dmaReplyRxIsr`) run in ISR context
2. ISR enqueues signals to `m_queue` (FIFO, max 4 entries)
3. `activeIn_handler()` runs in normal context (via PassiveCycler)
4. Handler dequeues all pending signals and invokes output ports
5. This ensures no F´ port calls occur within ISR context

### 3.8 Baud Rate Calculation

The driver calculates the USART BAUD register value based on mode:

**Asynchronous Mode (most common):**

```
BAUD = 65536 * (1 - S * (f_BAUD / f_ref))
```

where:

- `f_ref` = 48,000,000 Hz (GCLK0)
- `f_BAUD` = desired baud rate
- `S` = 16 (oversampling factor)

**Synchronous Mode:**

```
BAUD = f_ref / (2 * f_BAUD)
```

Range: 0-255 (asserted during configuration)

## 4. Usage

### 4.1 Configuration

The driver supports configuration of TX queue depth via `config/UsartDriverConfig.hpp`:

```cpp
namespace Samd21 {
enum UsartDriverConfig {
    USART_TX_BUFFER_N = 2,  // Number of pending TX buffers
};
}
```

Increasing `USART_TX_BUFFER_N` allows more TX requests to be queued simultaneously, reducing `SEND_RETRY` occurrences
under high TX load.

### 4.2 Common Configurations

**IMPORTANT:** GPIO pin muxing must be configured BEFORE calling `configure()`. Use `Samd21::PinMux::configure()` to
assign GPIO pins to the SERCOM peripheral function.

#### 4.2.1 Standard Async UART (115200 8N1, LSB-first)

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
    UsartDriver::Parity::NONE,
    256,   // RX buffer size (128 bytes per buffer)
    10     // Idle watchdog count (10 ticks = ~1.25s at 8Hz)
);
```

### 4.3 Initialization Sequence

**Prerequisite:** GPIO pins must be configured for SERCOM peripheral function using `Samd21::PinMux::configure()` before
calling `configure()`. The driver does not manage pin muxing internally.

The `configure()` method performs the following steps:

1. Validate configuration is called only once (`m_configured == false`)
2. Enable SERCOM peripheral clock via PM APBCMASK register
3. Assign GCLK0 (48MHz) to SERCOM via GCLK controller
4. Build CTRLA register (operating mode, communication mode, pin config, bit order, parity enable)
5. Build CTRLB register (character size, parity mode, stop bits)
6. Disable USART peripheral for configuration
7. Write CTRLA and CTRLB registers (enable-protected)
8. Calculate and write BAUD register (internal clock only)
9. Allocate RX buffers via `allocate_out()` port
10. Initialize RX state machine (A = `DMA`, B = `DMA_WAITING`)
11. Queue both RX buffers to DMA
12. Configure RX DMA for circular mode via `dmaRxCircular_out()`
13. Set `m_configured = true`
14. Enable USART peripheral
15. Enable transmitter and receiver (TXEN, RXEN bits)
16. Signal ready via `ready_out()` port

**Safety Features:**

- All register writes wait for `SYNCBUSY` to clear
- Configuration can only be called once (asserted)
- Watchdog count must be > 0 (asserted)
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

The `schedIn_handler()` decrements the idle watchdog and triggers partial RX frame extraction when the watchdog expires.

### 4.5 Topology Connections

```fpp
instance passiveCycler: Svc.PassiveCycler base id 0xD00
instance usartDriver: Samd21.UsartDriver base id 0xF00
instance dmacDriver: Samd21.DmacDriver base id 0xF10
instance staticMallocator: Svc.StaticMemAllocator base id 0xF20
instance rateGroup1Hz: Svc.ActiveRateGroup base id 0xF30

connections UsartComm {
  # Cycler drives the driver
  passiveCycler.cycleOut -> usartDriver.activeIn

  # Rate group provides idle watchdog ticks
  rateGroup1Hz.RateGroupMemberOut[0] -> usartDriver.schedIn

  # DMA channel connections
  usartDriver.dmaQueueOut[0] -> dmacDriver.jobIn[0]   # TX channel
  usartDriver.dmaQueueOut[1] -> dmacDriver.jobIn[1]   # RX channel
  dmacDriver.jobReplyOut[0] -> usartDriver.dmaReplyIn[0]
  dmacDriver.jobReplyOut[1] -> usartDriver.dmaReplyIn[1]
  usartDriver.dmaRxCircular -> dmacDriver.channelCircularIn[1]
  usartDriver.dmaRxPopCurrent -> dmacDriver.channelReadWritebackIn[1]

  # Buffer allocation for RX
  usartDriver.allocate -> staticMallocator.bufferAllocate
  usartDriver.deallocate -> staticMallocator.bufferDeallocate
}
```

### 4.6 Tested Configurations

This component has been tested on the following hardware venues:

| Board Name               | Chip       | SERCOM  | Config     | Baud   | Data Order |
|--------------------------|------------|---------|------------|--------|------------|
| Microchip Curiosity Nano | SAMD21G17A | SERCOM0 | Async, 8N1 | 115200 | LSB-first  |

## 5. Performance Characteristics

### 5.1 Timing

- **TX Latency**: Single DMA setup overhead (~10-20 CPU cycles) + DMA transfer time
- **RX Latency**: DMA transfer time + idle watchdog period (configurable, typically 1-2 rate group periods)
- **ISR Duration**: < 50 CPU cycles (signal enqueue only, no port calls)

### 5.2 Memory Usage

- **Static**: ~220 bytes (component state, queues)
- **Dynamic**: 0 bytes (all allocations via `allocate` port during `configure()`)
- **RX Buffers**: User-configured via `rx_buffer_size` parameter (typically 128-256 bytes total)
- **TX Queue**: `USART_TX_BUFFER_N * sizeof(ThinBuffer)` = `2 * 16` = 32 bytes

### 5.3 Limitations

- Maximum TX queue depth: `USART_TX_BUFFER_N` (configurable, default = 2)
- RX buffer overrun occurs if downstream consumer does not return buffers before both buffers fill
- Idle watchdog resolution limited by rate group frequency (e.g., 125ms at 8Hz)
- Component configuration is one-time only (no runtime reconfiguration)

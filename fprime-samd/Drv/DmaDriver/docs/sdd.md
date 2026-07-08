# Samd21::DmaDriver

## 1. Introduction

The `Samd21::DmaDriver` component provides a hardware driver for the SAMD21 Direct Memory Access Controller (DMAC) peripheral. This passive component manages all 12 hardware DMA channels, supporting memory-to-memory, peripheral-to-memory, and memory-to-peripheral transfers with configurable priorities, beat sizes, and trigger sources.

The driver implements a descriptor-based architecture with support for linked descriptor chains (up to 16 descriptors shared across all channels), enabling automatic sequential transaction processing without CPU intervention. Channels operate independently with interrupt-driven completion notifications and support both one-shot and circular buffer modes.

## 2. Requirements

| Name           | Description                                                                             | Validation    |
| -------------- | --------------------------------------------------------------------------------------- | ------------- |
| SAMD21-DMA-001 | The DmaDriver shall manage all 12 DMAC hardware channels independently                  | Hardware Test |
| SAMD21-DMA-002 | The DmaDriver shall support descriptor-based DMA transactions                           | Hardware Test |
| SAMD21-DMA-003 | The DmaDriver shall support linked descriptor chains for sequential transfers           | Hardware Test |
| SAMD21-DMA-004 | The DmaDriver shall support configurable DMA parameters (trigger sources, transaction types, priorities, beat sizes, address increment modes) | Hardware Test |
| SAMD21-DMA-005 | The DmaDriver shall detect and report bus errors via ISR output ports                   | Hardware Test |
| SAMD21-DMA-006 | The DmaDriver shall support circular buffer mode for continuous transfers               | Hardware Test |
| SAMD21-DMA-007 | The DmaDriver shall provide writeback register access for in-flight transfer monitoring | Hardware Test |
| SAMD21-DMA-008 | The DmaDriver shall report the number of free descriptors as telemetry                  | Hardware Test |

## 3. Design

### 3.1 Component Diagram

`Samd21::DmaDriver` is a passive component that manages the SAMD21 DMAC peripheral. It implements a per-channel port interface pattern where each port is an array of 12 elements, with port index selecting the hardware DMA channel (0-11).

### 3.2 Ports

| Kind          | Name                | Port Type              | Usage                                                                   |
| ------------- | ------------------- | ---------------------- | ----------------------------------------------------------------------- |
| `sync input`  | `sendTransactionIn` | `Dma.Transaction`      | Queue DMA transaction on channel (array of 12 ports)                    |
| `sync input`  | `linkToFrontIn`     | `Fw.Signal`            | Link last descriptor to first for circular mode (array of 12 ports)     |
| `sync input`  | `readWritebackIn`   | `Dma.ReadWriteback`    | Suspend, read the writeback descriptor, resume (array of 12 ports)      |
| `sync input`  | `schedIn`           | `Svc.Sched`            | Rate group tick; publishes the `freeDescriptors` telemetry channel      |
| `output`      | `transactionIsrOut` | `Dma.TransactionReply` | Transaction completion signal from ISR (array of 12 ports)              |

Port indices 0-11 map directly to DMAC hardware channels 0-11. Each channel operates independently — transactions queued on different channels execute in parallel according to hardware priority arbitration.

The component also imports `Fw.Channel` (with a `timeCaller` time-get port) to emit telemetry.

**Telemetry:**

| Channel           | Type | Description                                                        |
| ----------------- | ---- | ------------------------------------------------------------------ |
| `freeDescriptors` | `U8` | Number of unused descriptors in the shared pool, published on `schedIn` |

#### 3.2.1 Why Sync Ports?

The `sendTransactionIn` and related ports are `sync` (not `guarded`) because this is a baremetal deployment with no RTOS — there are no concurrent threads, only ISR context and main context. Mutex protection (`guarded`) would add unnecessary overhead with no concurrency benefit. The driver uses a global mutex internally to protect hardware register access when required.

### 3.3 Hardware Configuration

The `configure()` method initializes the DMAC peripheral:

| Feature           | Configuration                                                |
| ----------------- | ------------------------------------------------------------ |
| Clock Enable      | AHBMASK and APBBMASK via PM registers                        |
| Reset             | Software reset with timeout protection (waits up to F_CPU cycles) |
| Base Descriptors  | 16-byte aligned `dmac_base[12]` array (one per channel)     |
| Writeback Descriptors | 16-byte aligned `dmac_writeback[12]` array (hardware-updated) |
| Priority Levels   | All 4 levels enabled (DMAC_CTRL_LVLEN(0xF))                 |
| Debug Operation   | DMAC continues running during debug pause (DBGCTRL.DBGRUN=1) |
| Interrupt         | NVIC enabled at lowest priority ((1 << __NVIC_PRIO_BITS) - 1) |

The method must be called exactly once during topology initialization (in `configComponents` phase). Calling `configure()` when already initialized triggers an assertion failure.

### 3.4 Public Interface

#### 3.4.1 Transaction Parameters

The `sendTransactionIn` port accepts a `Dma.Transaction` struct with the following parameters:

```fpp
struct Transaction {
    trigger: Dma.TriggerSource          # Peripheral trigger (SERCOM, timer, ADC) or DISABLE
    action: Dma.TransactionType         # BEAT, BLOCK, or TRANSACTION
    priority: Dma.Priority              # Channel priority (0-3) for arbitration
    sourceAddr: U32                     # Source address (must be aligned to beat size)
    destAddr: U32                       # Destination address (must be aligned to beat size)
    len: U32                            # Transfer length in bytes (multiple of beat size)
    beatSize: Dma.BeatSize              # BYTE (8-bit), HWORD (16-bit), WORD (32-bit)
    incrementSource: bool               # Auto-increment source address
    incrementDestination: bool          # Auto-increment destination address
    stepSize: Dma.AddressIncrementStepSize  # Step increment (1, 2, 4, 8, 16, 32, 64, 128 bytes)
    stepSelection: Dma.StepSelection    # Apply step to SOURCE or DESTINATION
}
```

**Validation:** All parameters are validated with assertions:
- Source/destination addresses must be non-zero and aligned to beat size
- Transfer length must be non-zero and a multiple of beat size
- Beat count must be in range [1, 65535]
- All enum parameters are validated via their `.isValid()` methods

**Software Trigger Behavior:** When a channel is enabled, the driver issues a software trigger (`SWTRIGCTRL.reg |= (1 << channel_id)`) to start the transfer immediately, even if the configured peripheral trigger has already fired (per SAMD21 datasheet §21.8.8).

#### 3.4.2 Transaction Reply

The `transactionIsrOut` port returns a `Dma.Reply` struct on completion:

```fpp
struct Reply {
    status: Dma.Status        # OK or BUS_ERROR
    remainingBytes: U32       # Bytes not transferred (0 if successful)
}
```

This port is invoked **from ISR context** and must connect to a `sync input` handler that immediately returns (no blocking operations, no complex logic).

#### 3.4.3 Writeback State

The `readWritebackIn` port returns a `Dma.Writeback` struct providing in-flight transfer state:

```fpp
struct Writeback {
    btctrl: U32      # Block Transfer Control register (configuration snapshot)
    btcnt: U16       # Remaining beat count (decrements during transfer)
    srcaddr: U32     # Current source address (or end address if incrementing)
    dstaddr: U32     # Current destination address (or end address if incrementing)
    descaddr: U32    # Next descriptor address in chain (0 if last)
}
```

The writeback descriptor is updated live by hardware during DMA transfers. To read a coherent snapshot, `readWritebackIn` momentarily suspends the channel, reads the writeback registers, and then resumes the channel — the transfer continues from where it left off. This lets a consumer (e.g. the `UsartDriver` idle watchdog) read how many bytes have arrived in the buffer the DMA is actively filling without disturbing the descriptor chain.

> **Note:** The former `popFrontIn` port — which read the writeback and then forced the channel to advance to the next descriptor — has been removed. Consumers now read the in-flight byte count via `readWritebackIn` and extract data in place, leaving the descriptor chain untouched.

### 3.5 Descriptor Architecture

Each channel uses a descriptor-based transfer model:

- **Base descriptor** — Primary descriptor per channel (`dmac_base[channel_id]`), copied from queued transactions
- **Writeback descriptor** — Hardware-updated status (`dmac_writeback[channel_id]`), reflects current transfer state
- **Linked descriptors** — Shared pool of 16 descriptors for chaining across all channels (`m_descriptors[]`)

The descriptor pool is shared across all 12 channels. A 32-bit bitfield (`m_descriptors_used`) tracks which descriptors are in use. Descriptors are allocated when transactions are queued and freed when they complete (unless the channel is in circular mode).

**Automatic Chain Linking:** Transactions can be queued while a channel is active. The driver suspends the channel momentarily, links the new descriptor to the end of the chain, and resumes automatically. If the channel completes before linking (race condition detected via SUSP flag), the new descriptor becomes the new base descriptor and the channel restarts.

#### 3.5.1 Descriptor Lifecycle

1. **Allocation** — Driver searches `m_descriptors_used` bitfield for a free descriptor
2. **Configuration** — Descriptor populated with source/dest addresses, length, beat size, control flags
3. **Linking:**
   - If channel is idle: descriptor is copied to `dmac_base[channel_id]`, original freed immediately
   - If channel is busy: descriptor linked to chain via DESCADDR pointers, freed on completion
4. **Execution** — Hardware processes descriptors sequentially, updating writeback after each
5. **Completion** — On TCMPL interrupt, completed descriptor freed (unless circular mode)
6. **Error** — On TERR, current descriptor and all remaining chain descriptors freed (unless circular mode)

Base descriptors (`dmac_base[]`) are never freed—they belong to hardware-managed memory.

### 3.6 Interrupt Handling

The driver handles three interrupt types:

**Transfer Complete (TCMPL)** — Descriptor completed successfully. Frees the completed descriptor (unless circular mode), updates tracking to the next descriptor, and emits `transactionIsrOut` with status `OK` and `remainingBytes=0`. This fires for both intermediate descriptors in a chain and the final descriptor.

**Transfer Error (TERR)** — AHB bus error (invalid address, MPU violation, alignment fault). Emits `transactionIsrOut` with status `BUS_ERROR` and the remaining byte count (calculated from writeback BTCNT). The channel is disabled by hardware on error. All remaining descriptors in the chain are freed (unless circular mode).

**Suspend (SUSP)** — Channel reached end of descriptor chain (DESCADDR=0) or was manually suspended. When SUSP fires with FERR and DESCADDR=0, this indicates normal end-of-chain completion and the channel is marked idle. Invalid descriptors (FERR with DESCADDR≠0) trigger an assertion failure.

### 3.7 ISR Integration

The driver requires the `DMAC_Handler` ISR to be linked at compile time. This ISR is provided in `DmaDriverIsr.cpp` and calls `DmaDriver::handleInterrupt()` via the singleton instance pointer (`s_instance`). The ISR is registered in the NVIC during `configure()` with the lowest priority level.

**Singleton Pattern:** Only one `DmaDriver` instance is supported per application. The constructor asserts if `s_instance` is already set. This constraint is inherent to the singleton ISR integration pattern required for baremetal SAMD21 deployments.

## 4. Usage

### 4.1 Initialization

Initialize the driver during topology setup:

```fpp
instance dmaDriver: Samd21.DmaDriver base id 0xD00

phase Fpp.ToCpp.Phases.configComponents """
dmaDriver.configure();
"""
```

### 4.2 Memory-to-Memory Copy

```cpp
// Copy 256 bytes from srcBuffer to dstBuffer using 32-bit transfers
dmaDriver.sendTransactionIn_out(
    0,                                          // Channel 0
    Dma::TriggerSource::DISABLE,                // Software trigger only
    Dma::TransactionType::TRANSACTION,          // Full transfer on one trigger
    Dma::Priority::PRIORITY_0,                  // Lowest priority
    reinterpret_cast<U32>(srcBuffer),
    reinterpret_cast<U32>(dstBuffer),
    256,                                        // Length in bytes
    Dma::BeatSize::WORD,                        // 32-bit transfers (4 bytes/beat)
    true, true,                                 // Increment both addresses
    Dma::AddressIncrementStepSize::SIZE_1,      // Step by 1 beat (4 bytes)
    Dma::StepSelection::DESTINATION             // Step applied to destination (unused when both increment)
);
```

### 4.3 Peripheral Transfers (UART RX)

```cpp
// USART RX: Peripheral-to-memory, triggered by SERCOM hardware
dmaDriver.sendTransactionIn_out(
    1,                                          // Channel 1
    Dma::TriggerSource::SERCOM0_RX,             // Hardware trigger from SERCOM0 RX
    Dma::TransactionType::BEAT,                 // One beat (byte) per trigger
    Dma::Priority::PRIORITY_2,                  // Higher priority than mem-to-mem
    reinterpret_cast<U32>(&SERCOM0->USART.DATA.reg),  // Fixed peripheral register
    reinterpret_cast<U32>(rxBuffer),            // RAM destination
    64,                                         // Buffer size in bytes
    Dma::BeatSize::BYTE,                        // 8-bit transfers
    false, true,                                // Fixed source, increment dest
    Dma::AddressIncrementStepSize::SIZE_1,      // Step by 1 byte
    Dma::StepSelection::DESTINATION             // Step applied to destination
);
```

### 4.4 Linked Transactions

Submit multiple transactions to the same channel. The driver automatically chains them:

```cpp
// First transaction starts immediately
dmaDriver.sendTransactionIn_out(3, ..., buf1, dest, 128, ...);
// Second transaction appends to chain, starts when first completes
dmaDriver.sendTransactionIn_out(3, ..., buf2, dest+128, 256, ...);
// Third transaction appends to chain, starts when second completes
dmaDriver.sendTransactionIn_out(3, ..., buf3, dest+384, 64, ...);

// All three execute sequentially without CPU intervention
// Each emits transactionIsrOut when its descriptor completes
```

### 4.5 Circular Buffer with linkToFront

Configure a channel to loop continuously through a linked descriptor chain:

```cpp
// Queue multiple descriptors (minimum 2 required for circular mode)
dmaDriver.sendTransactionIn_out(2, ..., buf1, dest, 256, ...);
dmaDriver.sendTransactionIn_out(2, ..., buf2, dest, 256, ...);
dmaDriver.sendTransactionIn_out(2, ..., buf3, dest, 256, ...);

// Link last descriptor back to first for continuous looping
dmaDriver.linkToFrontIn_out(2);

// Channel now runs indefinitely: buf1 → buf2 → buf3 → buf1 → ...
// transactionIsrOut fires after each buffer completes
// Descriptors are never freed (reused continuously)
```

**Requirements:**
- Channel must be busy (have at least one transaction queued)
- Base descriptor must have at least one linked descriptor (DESCADDR ≠ 0)
- Once circular mode is enabled, no further transactions can be queued (assertion failure)
- Circular mode cannot be disabled (remains until channel reset)

### 4.6 In-Flight Transfer Monitoring with readWriteback

Read how far the active transfer has progressed. This is used, for example, by the `UsartDriver` idle watchdog to drain the buffer the RX DMA is currently filling without disturbing the descriptor chain:

```cpp
// On UART IDLE detection:
Dma::Writeback wb = dmaDriver.readWritebackIn_out(1);  // Channel 1

// Calculate bytes received so far in the current buffer
U32 beatsRemaining = wb.get_btcnt();
U32 bytesReceived  = originalBtcnt - beatsRemaining;

// Process the partial buffer in place; the DMA keeps filling the SAME buffer
if (bytesReceived > 0) {
    processUartData(currentBuffer, bytesReceived);
}

// Note: Addresses in writeback are END addresses if incrementing.
// To get the current address: subtract (beatsRemaining * beatSize).
```

**readWriteback Algorithm:**
1. Suspend the channel
2. Wait for CHINTFLAG.SUSP to confirm suspension
3. Read the writeback descriptor to capture a coherent snapshot
4. Resume the channel (the transfer continues into the same buffer)

**Requirements:**
- Channel must be busy (have an active transaction)

Unlike the removed `popFront`, `readWriteback` never advances the channel to the next descriptor — the active buffer stays in place and the consumer tracks its own read offset.

### 4.7 Free Descriptor Telemetry

Connect `schedIn` to a rate group to periodically publish the number of unused descriptors in the shared pool via the `freeDescriptors` telemetry channel. This is useful for monitoring descriptor-pool exhaustion in flight:

```fpp
rateGroup1Hz.RateGroupMemberOut[1] -> dmaDriver.schedIn
```

On each tick, the handler counts the clear bits in the `m_descriptors_used` bitfield and writes the count to `freeDescriptors`.

### 4.8 Topology Connections

```fpp
instance dmaDriver: Samd21.DmaDriver base id 0xD00
instance usartDriver: Samd21.UsartDriver base id 0xD10

connections DmaConnections {
  # USART TX uses DMA channel 0
  usartDriver.dmaQueueOut[Samd21.UsartDriver.DmaChannel.TX] -> dmaDriver.sendTransactionIn[0]
  dmaDriver.transactionIsrOut[0] -> usartDriver.dmaReplyIn[Samd21.UsartDriver.DmaChannel.TX]
  
  # USART RX uses DMA channel 1 (circular mode)
  usartDriver.dmaQueueOut[Samd21.UsartDriver.DmaChannel.RX] -> dmaDriver.sendTransactionIn[1]
  dmaDriver.transactionIsrOut[1] -> usartDriver.dmaReplyIn[Samd21.UsartDriver.DmaChannel.RX]
  usartDriver.dmaRxCircular -> dmaDriver.linkToFrontIn[1]
  usartDriver.dmaRxRead -> dmaDriver.readWritebackIn[1]

  # Rate group publishes the free-descriptor telemetry channel
  rateGroup1Hz.RateGroupMemberOut[1] -> dmaDriver.schedIn
}
```

Configuration phase setup (in `instances.fpp`):

```fpp
phase Fpp.ToCpp.Phases.configComponents """
dmaDriver.configure();
"""
```

### 4.9 Tested Configurations

This component has been tested on the following hardware venues:

| Board Name               | Chip       | Channels Tested | Use Case                |
| ------------------------ | ---------- | --------------- | ----------------------- |
| Microchip Curiosity Nano | SAMD21G17A | 0, 1            | USART TX/RX (circular)  |

## 5. Constraints and Limitations

### 5.1 Resource Limits

- **12 hardware channels** — Fixed by SAMD21 DMAC peripheral
- **16 descriptor pool** — Shared across all channels (configurable in `DmaDriverConfig::DMA_DESCRIPTOR_N`)
- **Maximum 32 descriptors** — Enforced by 32-bit bitfield used for tracking descriptor allocation
- **Beat count range** — [1, 65535] beats per descriptor (BTCNT is 16-bit)

When the descriptor pool is exhausted, `sendTransactionIn` asserts (no graceful degradation). Size the pool appropriately for the maximum number of concurrent queued transactions across all channels.

### 5.2 Circular Buffer Constraints

- **Irreversible** — Once `linkToFront` is called, the channel remains circular until reset
- **No further queueing** — Attempting to queue transactions after `linkToFront` triggers assertion
- **No descriptor freeing** — Descriptors in circular chains are never freed (remain allocated indefinitely)

### 5.3 Alignment Requirements

- **Descriptor memory** — Must be 16-byte aligned (enforced by `__attribute__((__aligned__(16)))`)
- **Source/destination addresses** — Must be aligned to beat size:
  - BYTE (8-bit): 1-byte alignment
  - HWORD (16-bit): 2-byte alignment (address must be even)
  - WORD (32-bit): 4-byte alignment (address must be multiple of 4)
- **Transfer length** — Must be a multiple of beat size

### 5.4 Thread Safety

- **No RTOS** — This driver is designed for baremetal deployments with no concurrent threads
- **Mutex protection** — Internal global mutex protects DMAC register access during transaction submission
- **ISR context** — `handleInterrupt()` runs without mutex protection (assumes no conflicting access from ISR)
- **Single instance** — Only one `DmaDriver` instance is supported per application (singleton ISR pattern)

### 5.5 Error Handling

- **Assertion-based validation** — Invalid parameters trigger assertions (FW_ASSERT) rather than error codes
- **No recovery from bus errors** — On TERR interrupt, the channel is disabled by hardware and all queued transactions are cancelled (returned with `BUS_ERROR` status)
- **Timeout protection** — Software reset and channel suspend operations have CPU cycle-based timeouts (F_CPU iterations) to prevent infinite loops

### 5.6 Hardware-Specific Behavior

- **SAMD21 specific** — Uses SAMD21 register definitions from `sam.h` (not portable to other MCUs)
- **Software trigger quirk** — Software trigger only fires if `CHSTATUS.PEND=0` (see SAMD21 datasheet §21.8.8)
- **Address end-pointing** — SAMD21 DMAC stores END addresses in SRCADDR/DSTADDR when increment is enabled (not current addresses)
- **Step size feature** — Step increments (SIZE_2 through SIZE_128) are for circular buffer stride patterns and audio interleaving

## 6. Performance Characteristics

### 6.1 Descriptor Allocation Overhead

- **Best case** — First descriptor found: ~10 CPU cycles
- **Worst case** — All 16 descriptors scanned: ~160 CPU cycles
- **Assertion** — No free descriptor: FW_ASSERT triggers (unbounded)

### 6.2 Transaction Submission Latency

- **Idle channel** — Descriptor setup + channel configuration: ~50 CPU cycles
- **Busy channel** — Descriptor setup + suspend + link + resume: ~200 CPU cycles
- **Race condition** — Channel completes during submission: +50 CPU cycles (restart path)

### 6.3 Memory Usage

- **Component state** — ~150 bytes (bitfield, channel state, descriptor pool)
- **Descriptor pool** — `16 * 16 bytes = 256 bytes` (configurable)
- **Hardware descriptors** — `2 * 12 * 16 bytes = 384 bytes` (base + writeback)
- **Total** — ~790 bytes per DmaDriver instance

### 6.4 Interrupt Latency

- **ISR entry** — NVIC overhead + context save: ~10 µs @ 48 MHz
- **ISR body** — Descriptor freeing + port call: ~20 µs @ 48 MHz
- **Total** — < 30 µs per interrupt (measurement needed)

Interrupt priority is set to lowest level during `configure()` to minimize impact on critical ISRs (e.g., hard fault, systick).

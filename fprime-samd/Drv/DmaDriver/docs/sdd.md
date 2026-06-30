# Samd21::DmaDriver

## 1. Introduction

The `Samd21::DmaDriver` component provides a hardware driver for the SAMD21 Direct Memory Access Controller (DMAC)
peripheral. This passive component manages all 12 hardware DMA channels, supporting memory-to-memory,
peripheral-to-memory, and memory-to-peripheral transfers with configurable priorities, beat sizes, and trigger sources.

The driver implements a descriptor-based architecture with support for linked descriptor chains (up to 16 total
descriptors shared across all channels), enabling automatic sequential transaction processing without CPU intervention. 
Channels operate independently with interrupt-driven completion notifications.

## 2. Requirements

| Name           | Description                                                                             | Validation    |
| -------------- | --------------------------------------------------------------------------------------- | ------------- |
| SAMD21-DMA-001 | The DmaDriver shall manage all 12 DMAC hardware channels independently                  | Hardware Test |
| SAMD21-DMA-002 | The DmaDriver shall support descriptor-based DMA transactions                           | Hardware Test |
| SAMD21-DMA-003 | The DmaDriver shall support linked descriptor chains for sequential transfers           | Hardware Test |
| SAMD21-DMA-004 | The DmaDriver shall support configurable DMA parameters including trigger sources (disable, SERCOM, timer, etc.), transaction types (beat, block, transaction), priorities (0-3), beat sizes (8-bit, 16-bit, 32-bit), and address increment modes | Hardware Test |
| SAMD21-DMA-005 | The DmaDriver shall detect and report bus errors via ISR output ports                   | Hardware Test |
| SAMD21-DMA-006 | The DmaDriver shall support popFront operations for partial buffer processing           | Hardware Test |
| SAMD21-DMA-007 | The DmaDriver shall provide writeback register access for live monitoring               | Hardware Test |
| SAMD21-DMA-008 | The DmaDriver shall emit completion signals via interrupt-driven ports                  | Hardware Test |

## 3. Design

### 3.1 Component Diagram

`Samd21::DmaDriver` is a passive component that manages the SAMD21 DMAC peripheral. It implements a per-channel port
interface pattern where port index selects the hardware channel.

### 3.2 Ports

| Kind            | Name                | Port Type              | Usage                                                                   |
| --------------- | ------------------- | ---------------------- | ----------------------------------------------------------------------- |
| `guarded input` | `sendTransactionIn` | `Dma.Transaction`      | Queue DMA transaction on channel (array of 12 ports)                    |
| `guarded input` | `linkToFrontIn`     | `Fw.Signal`            | Link last descriptor to first for looping (array of 12 ports)           |
| `guarded input` | `popFrontIn`        | `Dma.ReadWriteback`    | Suspend, read writeback, advance to next descriptor (array of 12 ports) |
| `sync input`    | `readWritebackIn`   | `Dma.ReadWriteback`    | Read writeback descriptor (array of 12 ports)                           |
| `output`        | `transactionIsrOut` | `Dma.TransactionReply` | Transaction completion signal from ISR (array of 12 ports)              |

Port indices 0-11 map directly to DMAC hardware channels 0-11.

### 3.3 Transaction Parameters

**DmaTransaction port** accepts the following parameters:

- **trigger** - Peripheral trigger source (SERCOM, timer, ADC, etc.) or DISABLE for software-only
- **action** - Transaction type: BEAT (one transfer per trigger), BLOCK (block per trigger), or TRANSACTION (entire
  transfer)
- **priority** - Channel priority (0-3) for arbitration
- **sourceAddr / destAddr** - Memory addresses (must be aligned to beat size)
- **len** - Transfer length in bytes
- **beatSize** - Transfer width: BYTE (8-bit), HWORD (16-bit), or WORD (32-bit)
- **incrementSource / incrementDestination** - Enable/disable address auto-increment
- **stepSize** - Step increment size (1, 2, 4, 8, 16, 32, 64, 128 bytes) for circular buffers
- **stepSelection** - Apply step to SOURCE or DESTINATION address

**Software Trigger Behavior**: When a channel is enabled, the driver issues a software trigger (`SWTRIGCTRL.reg |= (1 << 
channel_id)`) per datasheet §21.8.8. This generates a DMA trigger if `CHSTATUS.PEND=0`, allowing the transfer to start 
immediately even if the peripheral trigger condition has already been met and cleared.

**DmaReply struct** returned on completion:

- **status** - OK or BUS_ERROR
- **remainingBytes** - Bytes not transferred (0 if successful)

**DmaWriteback struct** provides live transfer state (read from hardware writeback descriptor):

- **btctrl** - Block Transfer Control register (read-only copy of configuration)
- **btcnt** - Remaining beat count (decrements as transfer progresses)
- **srcaddr** - Current source address (or end address if incrementing)
- **dstaddr** - Current destination address (or end address if incrementing)
- **descaddr** - Next descriptor address in the chain (0 if this is the last descriptor)

The writeback descriptor is updated live by hardware during DMA transfers and can be read via `readWritebackIn` port
without affecting the ongoing transfer.

### 3.4 Advanced Operations

**linkToFrontIn port** - Configures the last descriptor in a channel's linked chain to loop back to the first
descriptor. This enables continuous circular transfers where the channel automatically restarts from the beginning after
completing the last descriptor. Used for applications requiring seamless looping (e.g., continuous ADC sampling,
circular audio buffers).

Requirements:
- The channel must be busy (have at least one transaction already queued)
- The base descriptor must have at least one linked descriptor (DESCADDR != 0)
- Attempting to queue additional transactions after invoking linkToFront will trigger an assertion failure
- Once circular mode is enabled, it cannot be disabled (descriptors are never freed in circular mode)

**popFrontIn port** - Handles IDLE frame detection by suspending the channel, reading the writeback state, and advancing
to the next descriptor. This operation:

1. Suspends the channel (if not already suspended)
2. Waits for CHINTFLAG.SUSP to confirm suspension
3. Reads and returns the writeback state (caller can calculate bytes received as `original_BTCNT - writeback.btcnt`)
4. Sets writeback BTCNT to 0 to skip to the next descriptor
5. Resumes the channel

Requirements:
- The channel must be busy (have an active transaction)
- The channel must be in circular mode (popFront is only valid for circular buffers)

This is critical for UART protocols where IDLE detection requires processing a partial buffer and immediately switching
to an alternate buffer to avoid data loss.

### 3.5 Hardware Configuration

The `configure()` method configures the DMAC peripheral:

- Enables DMAC clocks (AHBMASK and APBBMASK via PM registers)
- Performs software reset with timeout protection (waits for DMAC->CTRL.bit.SWRST to clear)
- Configures 16-byte aligned base and writeback descriptor memory
  - Base descriptors: `dmac_base[12]` (one per channel, placed in `SECTION_DMAC_DESCRIPTOR`)
  - Writeback descriptors: `dmac_writeback[12]` (hardware-updated, placed in `SECTION_DMAC_DESCRIPTOR`)
- Enables all 4 priority levels (DMAC_CTRL_LVLEN(0xF))
- Enables DMAC interrupt in NVIC at lowest priority ((1 << __NVIC_PRIO_BITS) - 1)

The method must be called exactly once before any transactions are submitted. Calling `configure()` when already
initialized triggers an assertion failure.

### 3.6 Descriptor Architecture

Each channel uses a descriptor-based transfer model:

- **Base descriptor** - Primary descriptor per channel (`dmac_base[channel_id]`)
- **Writeback descriptor** - Hardware-updated status (`dmac_writeback[channel_id]`)
- **Linked descriptors** - Shared pool of 16 descriptors for chaining across all channels (`m_descriptors[]`)

The descriptor pool is shared across all 12 channels. A 32-bit bitfield (`m_descriptors_used`) tracks which descriptors
are in use. Descriptors are allocated when transactions are queued and freed when they complete (unless the channel is
in circular mode).

Transactions can be queued while a channel is active. The driver suspends the channel momentarily, links the new
descriptor to the chain, and resumes automatically. If the channel completes before the new descriptor can be linked
(race condition detected via SUSP flag), the new descriptor becomes the new base descriptor and the channel restarts.

#### Descriptor Lifecycle

1. **Allocation**: When a transaction is queued, the driver searches `m_descriptors_used` for a free descriptor
2. **Configuration**: The descriptor is populated with source/dest addresses, length, beat size, and control flags
3. **Linking**: 
   - If channel is idle: descriptor is copied to `dmac_base[channel_id]` and the copy is freed immediately
   - If channel is busy: descriptor is linked to the chain via DESCADDR pointers
4. **Execution**: Hardware processes descriptors sequentially, updating writeback after each descriptor completes
5. **Completion**: On TCMPL interrupt, the completed descriptor is freed (unless circular mode)
6. **Error handling**: On TERR, the current descriptor and all remaining chain descriptors are freed (unless circular mode)

Base descriptors (`dmac_base[]`) are never freed—they belong to hardware-managed memory.

### 3.7 Interrupt Handling

The driver handles three interrupt types:

**Transfer Complete (TCMPL)** - Descriptor completed successfully. Frees the completed descriptor (unless circular mode),
updates tracking to the next descriptor, and emits `transactionIsrOut` with status OK and remainingBytes=0. This fires
for both intermediate descriptors in a chain and the final descriptor.

**Transfer Error (TERR)** - AHB bus error (invalid address, MPU violation). Emits `transactionIsrOut` with status
BUS_ERROR and the remaining byte count. The channel is disabled by hardware on error. All remaining descriptors in the
chain are freed (unless circular mode).

**Suspend (SUSP)** - Channel reached end of descriptor chain (DESCADDR=0) or was manually suspended. When SUSP fires
with FERR and DESCADDR=0, this indicates normal end-of-chain completion and the channel is marked idle. Invalid
descriptors (FERR with DESCADDR≠0) trigger an assertion failure.

### 3.8 Thread Safety

A global mutex protects DMAC register access when submitting transactions or performing popFront operations from
multiple F´ threads. ISR context reads registers without mutex protection.

### 3.9 ISR Integration

The driver requires the `DMAC_Handler` ISR to be linked at compile time. This ISR is provided in `DmaDriverIsr.cpp` and
calls `DmaDriver::handleInterrupt()` via the singleton instance pointer (`s_instance`). The ISR is registered in the
NVIC during `configure()` with the lowest priority level.

### 3.10 Parameter Validation

All input parameters are validated with assertions:
- Source and destination addresses must be non-zero and aligned to the beat size
- Transfer length must be non-zero and a multiple of the beat size
- Beat count must be in range [1, 65535]
- Enum parameters (trigger, action, priority, beat size, step size, step selection) are validated
- Channel index must be valid [0, 11]

## 4. Usage

### 4.1 Initialization

Initialize the driver during topology setup:

```cpp
instance dmaDriver: Samd21.DmaDriver base id 0xD00

// In configComponents phase
dmaDriver.configure();
```

### 4.2 Memory-to-Memory Copy

```cpp
dmaDriver.sendTransactionIn_out(
    0,                                          // Channel 0
    Dma::TriggerSource::DISABLE,                // Software trigger
    Dma::TransactionType::TRANSACTION,          // Full transfer
    Dma::Priority::PRIORITY_0,
    reinterpret_cast<U32>(srcBuffer),
    reinterpret_cast<U32>(dstBuffer),
    256,                                        // Length in bytes
    Dma::BeatSize::WORD,                        // 32-bit transfers
    true, true,                                 // Increment both addresses
    Dma::AddressIncrementStepSize::SIZE_1,
    Dma::StepSelection::DESTINATION
);
```

### 4.3 Peripheral Transfers

```cpp
// USART RX: Peripheral-to-memory
dmaDriver.sendTransactionIn_out(
    1,
    Dma::TriggerSource::SERCOM0_RX,             // Hardware trigger
    Dma::TransactionType::BEAT,                 // One byte per trigger
    Dma::Priority::PRIORITY_2,
    reinterpret_cast<U32>(&SERCOM0->USART.DATA.reg),
    reinterpret_cast<U32>(rxBuffer),
    64,
    Dma::BeatSize::BYTE,
    false, true,                                // Fixed source, increment dest
    Dma::AddressIncrementStepSize::SIZE_1,
    Dma::StepSelection::DESTINATION
);
```

### 4.4 Linked Transactions

Submit multiple transactions to the same channel. The driver allocates descriptors from a shared pool of 16 descriptors
(across all channels) that execute sequentially without CPU intervention:

```cpp
// First starts immediately
dmaDriver.sendTransactionIn_out(3, ..., buf1, output, 128, ...);
// Second appends to chain
dmaDriver.sendTransactionIn_out(3, ..., buf2, output+128, 256, ...);
// Third appends to chain
dmaDriver.sendTransactionIn_out(3, ..., buf3, output+384, 64, ...);
```

### 4.5 Circular Buffer with linkToFront

Configure a channel to loop continuously through a linked descriptor chain:

```cpp
// Queue multiple descriptors
dmaDriver.sendTransactionIn_out(2, ..., buf1, dest, 256, ...);
dmaDriver.sendTransactionIn_out(2, ..., buf2, dest, 256, ...);
dmaDriver.sendTransactionIn_out(2, ..., buf3, dest, 256, ...);

// Link last descriptor back to first for continuous looping
dmaDriver.linkToFrontIn_out(2);
```

### 4.6 IDLE Frame Detection with popFront

Handle UART IDLE detection by processing partial buffers:

```cpp
// On IDLE interrupt from UART peripheral (must be in circular buffer mode):
Dma::Writeback wb = dmaDriver.popFrontIn_out(1);  // Channel 1

// Calculate bytes received in current buffer
U32 bytesReceived = originalBtcnt - wb.get_btcnt();

// Process the partial buffer
processUartData(currentBuffer, bytesReceived);

// Channel has automatically advanced to next descriptor (alternate buffer)
```

### 4.7 Topology Connections

```fpp
instance dmaDriver: Samd21.DmaDriver base id 0xD00
instance usartDriver: Samd21.UsartDriver base id 0xD10

connections DmaConnections {
  # USART TX uses DMA channel 0
  usartDriver.dmaRequestOut[0] -> dmaDriver.sendTransactionIn[0]
  dmaDriver.transactionIsrOut[0] -> usartDriver.dmaCompleteIn[0]
  
  # USART RX uses DMA channel 1
  usartDriver.dmaRequestOut[1] -> dmaDriver.sendTransactionIn[1]
  dmaDriver.transactionIsrOut[1] -> usartDriver.dmaCompleteIn[1]
  usartDriver.readWritebackOut -> dmaDriver.readWritebackIn[1]
}
```

Configuration phase setup (in `instances.fpp`):

```fpp
phase Fpp.ToCpp.Phases.configComponents """
dmaDriver.configure();
"""
```

## 5. Constraints and Limitations

### 5.1 Resource Limits

- **12 hardware channels**: Fixed by SAMD21 DMAC peripheral
- **16 descriptor pool**: Shared across all channels, configured in `DmaDriverConfig::DMA_DESCRIPTOR_N`
- **Maximum 32 descriptors**: Enforced by 32-bit bitfield used for tracking descriptor allocation
- **Beat count range**: [1, 65535] beats per descriptor (BTCNT is 16-bit)

### 5.2 Circular Buffer Constraints

- **Irreversible**: Once `linkToFront` is called, the channel remains circular until explicitly disabled/reset
- **No further queueing**: Attempting to queue transactions after `linkToFront` triggers assertion failure
- **No descriptor freeing**: Descriptors in circular chains are never freed (remain allocated until channel reset)
- **popFront requirement**: `popFront` can only be called on circular buffer channels

### 5.3 Alignment Requirements

- **Descriptor memory**: Must be 16-byte aligned (enforced by `__attribute__((__aligned__(16)))`)
- **Source/destination addresses**: Must be aligned to beat size (8-bit: 1-byte, 16-bit: 2-byte, 32-bit: 4-byte)
- **Transfer length**: Must be a multiple of beat size

### 5.4 Thread Safety

- **Mutex protection**: All port handlers (except ISR) acquire a global mutex before accessing DMAC registers
- **ISR context**: `handleInterrupt()` runs without mutex protection—assumes no conflicting register access from ISR
- **Single instance**: Only one `DmaDriver` instance is supported per application (singleton pattern for ISR access)

### 5.5 Error Handling

- **Assertion-based validation**: Invalid parameters trigger assertions (FW_ASSERT) rather than returning error codes
- **No recovery from bus errors**: On TERR interrupt, the channel is disabled by hardware and must be reconfigured
- **Timeout protection**: Software reset and channel suspend operations have CPU cycle-based timeouts (F_CPU iterations)

### 5.6 Hardware-Specific Behavior

- **SAMD21 specific**: Uses SAMD21 register definitions from `sam.h` (not portable to other MCUs)
- **Step size feature**: Step increments (SIZE_2 through SIZE_128) are for circular buffer stride patterns
- **Software trigger quirk**: Software trigger only fires if `CHSTATUS.PEND=0` (see datasheet §21.8.8)

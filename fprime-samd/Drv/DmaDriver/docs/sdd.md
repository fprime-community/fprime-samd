# Samd21::DmaDriver

## 1. Introduction

The `Samd21::DmaDriver` component provides a hardware driver for the SAMD21 Direct Memory Access Controller (DMAC) peripheral. This passive component manages all 12 hardware DMA channels, supporting memory-to-memory, peripheral-to-memory, and memory-to-peripheral transfers with configurable priorities, beat sizes, and trigger sources.

The driver implements a descriptor-based architecture with support for linked descriptor chains (up to 4 additional descriptors per channel), enabling automatic sequential transaction processing without CPU intervention. Channels operate independently with interrupt-driven completion notifications.

## 2. Requirements

| Name            | Description                                                                  | Validation    |
| --------------- | ---------------------------------------------------------------------------- | ------------- |
| SAMD21-DMA-001  | The DmaDriver shall manage all 12 DMAC hardware channels independently      | Hardware Test |
| SAMD21-DMA-002  | The DmaDriver shall support descriptor-based DMA transactions                | Hardware Test |
| SAMD21-DMA-003  | The DmaDriver shall support linked descriptor chains for sequential transfers| Hardware Test |
| SAMD21-DMA-004  | The DmaDriver shall support configurable trigger sources (disable, SERCOM, timer, etc.) | Hardware Test |
| SAMD21-DMA-005  | The DmaDriver shall support configurable transaction types (beat, block, transaction) | Hardware Test |
| SAMD21-DMA-006  | The DmaDriver shall support configurable priorities (0-3)                    | Hardware Test |
| SAMD21-DMA-007  | The DmaDriver shall support 8-bit, 16-bit, and 32-bit beat sizes            | Hardware Test |
| SAMD21-DMA-008  | The DmaDriver shall support configurable address increment modes             | Hardware Test |
| SAMD21-DMA-009  | The DmaDriver shall detect and report bus errors via ISR output ports        | Hardware Test |
| SAMD21-DMA-010  | The DmaDriver shall support popFront operations for partial buffer processing | Hardware Test |
| SAMD21-DMA-011  | The DmaDriver shall provide writeback register access for live monitoring    | Hardware Test |
| SAMD21-DMA-012  | The DmaDriver shall emit completion signals via interrupt-driven ports       | Hardware Test |

## 3. Design

### 3.1 Component Diagram

`Samd21::DmaDriver` is a passive component that manages the SAMD21 DMAC peripheral. It implements a per-channel port interface pattern where port index selects the hardware channel.

### 3.2 Ports

| Kind     | Name                  | Port Type              | Usage                                                     |
| -------- | --------------------- | ---------------------- | --------------------------------------------------------- |
| `guarded input` | `sendTransactionIn`   | `Dma.Transaction`       | Queue DMA transaction on channel (array of 12 ports)      |
| `guarded input` | `linkToFrontIn`       | `Fw.Signal`            | Link last descriptor to first for looping (array of 12 ports) |
| `guarded input` | `popFrontIn`          | `Dma.ReadWriteback`    | Suspend, read writeback, advance to next descriptor (array of 12 ports) |
| `sync input` | `readWritebackIn`     | `Dma.ReadWriteback`     | Read writeback descriptor (array of 12 ports)             |
| `output` | `transactionIsrOut`   | `Dma.TransactionReply`  | Transaction completion signal from ISR (array of 12 ports)|

Port indices 0-11 map directly to DMAC hardware channels 0-11.

### 3.3 Transaction Parameters

**DmaTransaction port** accepts the following parameters:

- **trigger** - Peripheral trigger source (SERCOM, timer, ADC, etc.) or DISABLE for software-only
- **action** - Transaction type: BEAT (one transfer per trigger), BLOCK (block per trigger), or TRANSACTION (entire transfer)
- **priority** - Channel priority (0-3) for arbitration
- **sourceAddr / destAddr** - Memory addresses (must be aligned to beat size)
- **len** - Transfer length in bytes
- **beatSize** - Transfer width: BYTE (8-bit), HWORD (16-bit), or WORD (32-bit)
- **incrementSource / incrementDestination** - Enable/disable address auto-increment
- **stepSize** - Step increment size (1, 2, 4, 8, 16, 32, 64, 128 bytes) for circular buffers
- **stepSelection** - Apply step to SOURCE or DESTINATION address

**DmaReply struct** returned on completion:

- **status** - OK or BUS_ERROR
- **remainingBytes** - Bytes not transferred (0 if successful)

**DmaWriteback struct** provides live transfer state:

- **btcnt** - Remaining beat count
- **srcaddr / dstaddr** - Current addresses
- **descaddr** - Next descriptor address (0 if last)

### 3.4 Advanced Operations

**linkToFrontIn port** - Configures the last descriptor in a channel's linked chain to loop back to the first descriptor. This enables continuous circular transfers where the channel automatically restarts from the beginning after completing the last descriptor. Used for applications requiring seamless looping (e.g., continuous ADC sampling, circular audio buffers).

**popFrontIn port** - Handles IDLE frame detection by suspending the channel, reading the writeback state, and advancing to the next descriptor. This operation:
1. Suspends the channel (if not already suspended)
2. Waits for CHINTFLAG.SUSP to confirm suspension
3. Reads and returns the writeback state (caller can calculate bytes received as `original_BTCNT - writeback.btcnt`)
4. Sets writeback BTCNT to 0 to skip to the next descriptor
5. Resumes the channel

This is critical for UART protocols where IDLE detection requires processing a partial buffer and immediately switching to an alternate buffer to avoid data loss.

### 3.5 Hardware Configuration

The `init()` method configures the DMAC peripheral:
- Enables DMAC clocks (AHBMASK and APBBMASK)
- Performs software reset with timeout protection
- Configures 16-byte aligned base and writeback descriptor memory
- Enables all 4 priority levels
- Enables DMAC interrupt in NVIC at lowest priority

### 3.6 Descriptor Architecture

Each channel uses a descriptor-based transfer model:

- **Base descriptor** - Primary descriptor per channel (`dmac_base[channel_id]`)
- **Writeback descriptor** - Hardware-updated status (`dmac_writeback[channel_id]`)
- **Linked descriptors** - Per-channel pool of 4 additional descriptors for chaining (`m_descriptorPool`)

Transactions can be queued while a channel is active. The driver suspends the channel momentarily, links the new descriptor to the chain, and resumes automatically.

### 3.7 Interrupt Handling

The driver handles two interrupt types:

**Transfer Complete (TCMPL)** - Descriptor completed successfully, emits `transactionIsrOut` with status OK

**Transfer Error (TERR)** - AHB bus error (invalid address, MPU violation), emits `transactionIsrOut` with status BUS_ERROR and remaining byte count

### 3.8 Thread Safety

A global mutex protects DMAC register access when submitting transactions or performing popFront operations from multiple F´ threads. ISR context reads registers without mutex protection.

## 4. Usage

### 4.1 Initialization

Initialize the driver during topology setup:

```cpp
instance dmaDriver: Samd21.DmaDriver base id 0xD00

// After component construction
dmaDriver.init();
```

### 4.2 Memory-to-Memory Copy

```cpp
dmaDriver.sendTransactionIn_out(
    0,                                          // Channel 0
    DmaDriver_TriggerSource::DISABLE,           // Software trigger
    DmaDriver_TransactionType::TRANSACTION,     // Full transfer
    DmaDriver_Priority::PRIORITY_0,
    reinterpret_cast<U32>(srcBuffer),
    reinterpret_cast<U32>(dstBuffer),
    256,                                        // Length in bytes
    DmaDriver_BeatSize::WORD,                   // 32-bit transfers
    true, true,                                 // Increment both addresses
    DmaDriver_AddressIncrementStepSize::SIZE_1,
    DmaDriver_StepSelection::DESTINATION
);
```

### 4.3 Peripheral Transfers

```cpp
// USART RX: Peripheral-to-memory
dmaDriver.sendTransactionIn_out(
    1,
    DmaDriver_TriggerSource::SERCOM0_RX,        // Hardware trigger
    DmaDriver_TransactionType::BEAT,            // One byte per trigger
    DmaDriver_Priority::PRIORITY_2,
    reinterpret_cast<U32>(&SERCOM0->USART.DATA.reg),
    reinterpret_cast<U32>(rxBuffer),
    64,
    DmaDriver_BeatSize::BYTE,
    false, true,                                // Fixed source, increment dest
    DmaDriver_AddressIncrementStepSize::SIZE_1,
    DmaDriver_StepSelection::DESTINATION
);
```

### 4.4 Linked Transactions

Submit multiple transactions to the same channel. The driver queues up to 4 additional descriptors that execute sequentially without CPU intervention:

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
// On IDLE interrupt from UART peripheral:
Dma::Writeback wb = dmaDriver.popFrontIn_out(1);  // Channel 1

// Calculate bytes received in current buffer
U32 bytesReceived = originalBtcnt - wb.btcnt;

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

## 5. Configuration

The driver requires no compile-time configuration. All transfer parameters are specified per-transaction via the `sendTransactionIn` port.

This component has been tested on the following hardware venues:

| Board Name | Chip       | Test Case                          | Trigger Source |
| ---------- | ---------- | ---------------------------------- | -------------- |
| QtPy       | SAMD21E18A | Memory-to-memory 256B copy         | Software       |
| QtPy       | SAMD21E18A | SERCOM0 USART RX (circular buffer) | SERCOM0_RX     |
| QtPy       | SAMD21E18A | Linked chain (3 descriptors)       | Software       |

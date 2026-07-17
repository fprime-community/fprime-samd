# Samd21::RtcDriver

## 1. Introduction

The `Samd21::RtcDriver` component provides a hardware driver for the SAMD21 Real-Time Clock (RTC) peripheral configured in periodic interrupt mode. This passive component drives rate group execution using preset tick rates (1-128 Hz) and implements the F´ OSAL `Os::RawTime` interface for microsecond-resolution system timestamps.

The driver configures the RTC to generate periodic interrupts while remaining operational during SAMD21 standby modes. The component is cycled via the `activeIn` port (typically connected to a `Svc::PassiveCycler` component), which checks for RTC interrupts and emits cycle signals when they occur. Time measurement combines a monotonic tick counter with the RTC's 32-bit hardware counter (32768 Hz) to provide accurate timestamps with sub-tick precision.

## 2. Requirements

| Name           | Description                                                              | Validation    |
| -------------- | ------------------------------------------------------------------------ | ------------- |
| SAMD21-RTC-001 | The RtcDriver shall configure the RTC peripheral for periodic interrupts | Hardware Test |
| SAMD21-RTC-002 | The RtcDriver shall support configurable interrupt periods (1-128 Hz)    | Hardware Test |
| SAMD21-RTC-003 | The RtcDriver shall remain operational during SAMD21 standby modes       | Hardware Test |
| SAMD21-RTC-004 | The RtcDriver shall detect RTC interrupts via activeIn port              | Hardware Test |
| SAMD21-RTC-005 | The RtcDriver shall support internal, external, and ULP clock sources    | Hardware Test |
| SAMD21-RTC-006 | The RtcDriver shall emit cycle signals via the CycleOut port             | Hardware Test |
| SAMD21-RTC-007 | The RtcDriver shall provide an Os::RawTime implementation                | Hardware Test |
| SAMD21-RTC-008 | The RtcDriver shall detect when processing exceeds the configured period | Hardware Test |

## 3. Design

### 3.1 Component Diagram

`Samd21::RtcDriver` is a passive component that implements the `Drv.Tick` interface.

### 3.2 Ports

| Kind          | Name         | Port Type         | Usage                                            |
| ------------- | ------------ | ----------------- | ------------------------------------------------ |
| `sync input`  | `activeIn`   | `Svc.Sched`       | Cycled by PassiveCycler to poll for RTC interrupts |
| `output`      | `CycleOut`   | `Svc.Cycle`       | Periodic timing signal sent to rate group driver |
| `output`      | `timeGetOut` | `Fw.Time`         | Time get port for timestamp requests             |
| `output`      | `tlmOut`     | `Fw.Tlm`          | Telemetry port for emitting telemetry channels   |

#### 3.2.1 activeIn Port Pattern

The `activeIn` port is the key mechanism for driving the RtcDriver in a passive, baremetal system without an RTOS. This port must be connected to a `Svc::PassiveCycler` component which repeatedly invokes all connected components from the main execution loop.

**Port Handler Behavior:**

On each `activeIn_handler` invocation:
1. **Watchdog Reset**: Sets `deadline_reached = true` to signal the ISR that processing completed before the next interrupt
2. **Interrupt Poll**: Checks the `wakeup_interrupt` flag set by the RTC ISR
3. **Cycle Emission**: If an RTC interrupt occurred:
   - Clears the `wakeup_interrupt` flag to acknowledge the interrupt
   - Captures the current timestamp via `Os::RawTime::now()`
   - Emits `CycleOverrun` telemetry with the current overrun count
   - Calls `CycleOut_out(0, cycle_start)` to trigger downstream rate groups
4. **Return**: Returns immediately if no interrupt occurred (no-op cycle)

**Why This Pattern:**

In baremetal F´ deployments, active components (threads) are not available. The PassiveCycler provides a polling mechanism that allows passive components to be "driven" by the main loop while still responding to hardware interrupts efficiently. The RTC hardware interrupt sets a flag (`wakeup_interrupt`), and the main loop polls this flag via `activeIn` to determine when to emit timing signals.

This pattern separates interrupt handling (minimal ISR that sets a flag) from signal processing (full F´ port invocations with timestamps and telemetry), keeping ISR latency low while maintaining clean component boundaries.

### 3.3 Telemetry

| Name           | Type  | Update       | Description                                |
| -------------- | ----- | ------------ | ------------------------------------------ |
| `CycleOverrun` | `U16` | On change    | Counter tracking the number of cycle overruns |

### 3.4 Hardware Configuration

The driver configures the SAMD21 RTC peripheral (MODE0, 32-bit counter) with the following features:

| Feature           | Configuration                                                |
| ----------------- | ------------------------------------------------------------ |
| Clock Source      | Internal OSC32K, External XOSC32K, or ULP OSCULP32K          |
| Clock Routing     | Via GCLK4 with no division (DIV1)                            |
| Counter Mode      | Match-clear mode (resets on compare match)                   |
| Prescaler         | DIV1 (PRESCALER(0) - no prescaling for sub-tick accuracy)    |
| Compare Value     | `clock_freq_hz / tick_rate` (typically 32768 / tick_rate)    |
| Interrupt         | CMP0 (compare match interrupt), NVIC priority 0 (highest)    |
| Standby Operation | Clock source remains enabled during standby for WFI wake     |

### 3.5 Public Interface

```cpp
enum TickRate {
    TICK_1_HZ = 1, TICK_2_HZ = 2, TICK_4_HZ = 4, TICK_8_HZ = 8,
    TICK_16_HZ = 16, TICK_32_HZ = 32, TICK_64_HZ = 64, TICK_128_HZ = 128,
};

enum ClockSource {
    InternalOscillator,        // OSC32K (~32 kHz, ±3%, low power)
    ExternalOscillator,        // XOSC32K (32.768 kHz, ±20 ppm, high accuracy)
    UltraLowPowerOscillator,   // OSCULP32K (~32 kHz, ±10%, lowest power)
};

void configure(ClockSource clk_source, TickRate rate);  // Configure RTC peripheral and clock
void enable();                                          // Enable RTC interrupt and peripheral
void activeIn_handler(FwIndexType portNum, U32 context); // Port handler: checks for RTC interrupt, emits CycleOut
```

### 3.6 Os::RawTime Implementation

The driver provides `Os::Samd21::Samd21RawTime`, which implements the OSAL `RawTimeInterface`:

- **Monotonic tick counter** (`rtc_n_ticks`): Incremented on each RTC interrupt
- **32-bit RTC counter**: Provides sub-tick precision at configured clock rate
- **Clock rate tracking** (`rtc_clock_rate_hz`): Set during configuration based on clock source

Timestamps combine the coarse tick counter (seconds-level precision) with the hardware counter value (microsecond-level precision) to provide accurate time measurements. The implementation accounts for the configured clock rate, supporting non-standard external crystal frequencies.

### 3.7 Cycle Overrun Detection

A watchdog mechanism detects when rate group processing exceeds the configured period:

**Mechanism:**
- On each `activeIn_handler` call, `deadline_reached` flag is set to `true`
- RTC interrupt handler checks this flag - if `false`, rate groups are still processing from the previous cycle
- When overrun detected, `deadline_exceeded` counter (U16) increments
- The `CycleOverrun` telemetry channel reports this counter value on every RTC interrupt
- Counter persists across cycles to track total overruns since boot

**Telemetry:**
- Emitted on every RTC interrupt via `tlmWrite_CycleOverrun(deadline_exceeded)`
- Ground systems can monitor for increases to detect timing constraint violations
- Counter wraps at 65535 (U16 overflow)

## 4. Usage

### 4.1 Clock Source Selection

| Clock Source                 | Frequency  | Accuracy | Power    | Use Case                          |
| ---------------------------- | ---------- | -------- | -------- | --------------------------------- |
| OSC32K (Internal Oscillator) | ~32 kHz    | ±3%      | Low      | Development, less critical timing |
| XOSC32K (External Crystal)   | 32.768 kHz | ±20 ppm  | Medium   | High-accuracy production systems  |
| OSCULP32K (ULP Oscillator)   | ~32 kHz    | ±10%     | Very Low | Ultra low-power applications      |

**External Clock Configuration:**

The external oscillator frequency is configurable via `config/RtcDriverConfig.hpp`:

```cpp
namespace Samd21 {
namespace RtcDriverConfig {
    constexpr U32 EXTERNAL_CLOCK_FREQ_HZ = 32768;  // Adjust for non-standard crystals
}
}
```

The driver uses:
- `INTERNAL_CLOCK_FREQ_HZ = 32768` for internal and ULP oscillators (fixed)
- `RtcDriverConfig::EXTERNAL_CLOCK_FREQ_HZ` for external crystals (configurable)

This allows projects to use non-standard external crystal frequencies while maintaining accurate RawTime calculations.

### 4.2 Supported Tick Rates

| Tick Rate   | Frequency | Period    | RTC Compare Value |
| ----------- | --------- | --------- | ----------------- |
| TICK_1_HZ   | 1 Hz      | 1000 ms   | 32768             |
| TICK_2_HZ   | 2 Hz      | 500 ms    | 16384             |
| TICK_4_HZ   | 4 Hz      | 250 ms    | 8192              |
| TICK_8_HZ   | 8 Hz      | 125 ms    | 4096              |
| TICK_16_HZ  | 16 Hz     | 62.5 ms   | 2048              |
| TICK_32_HZ  | 32 Hz     | 31.25 ms  | 1024              |
| TICK_64_HZ  | 64 Hz     | 15.625 ms | 512               |
| TICK_128_HZ | 128 Hz    | 7.8125 ms | 256               |

#### 4.2.1 Tested configurations

This component has been tested on the following hardware venues:

| Board Name     | Chip       | Tick rate | Clock source    |
|----------------|------------| --------- | --------------- |
| QtPy           | SAMD21E18A | 8Hz       | Ultra low power |
| Curiosity Nano | SAMD21G17A | 8Hz       | Ultra low power |

### 4.3 Initialization

Configure and enable the driver during topology setup:

```cpp
// Configure RTC with external crystal at 16Hz
rtcDriver.configure(Samd21::RtcDriver::ExternalOscillator, 
                    Samd21::RtcDriver::TICK_16_HZ);

// Enable the RTC peripheral and interrupt
rtcDriver.enable();
```

The `configure()` method sets up the clock sources, GCLK routing, and RTC peripheral registers but leaves the RTC disabled. The `enable()` method then enables the NVIC interrupt (at highest priority) and starts the RTC peripheral. This two-step initialization allows configuration to be verified before enabling interrupts.

**Configuration Steps:**
1. Validate tick rate is one of the predefined enum values
2. Configure GCLK4 divisor to 1 (no division)
3. Select clock source (OSC32K, XOSC32K, or OSCULP32K) and set `rtc_clock_rate_hz`
4. Enable GCLK4 generator
5. Route GCLK4 to RTC peripheral
6. Enable RTC clock via GCLK controller
7. Power RTC via Power Manager (APBAMASK)
8. Configure RTC MODE0 with match-clear and PRESCALER(0)
9. Set compare register: `COMP[0] = rtc_clock_rate_hz / tick_rate`
10. Enable CMP0 interrupt at peripheral level

**Safety Features:**
- All synchronization waits include timeout assertions (limit = F_CPU iterations)
- Assert fires if GCLK or RTC peripheral synchronization times out
- Prevents infinite loops if hardware fails to respond

### 4.4 Driving the Component

#### 4.4.1 PassiveCycler Integration

The RtcDriver requires a `Svc::PassiveCycler` component to drive its `activeIn` port from the main execution loop. The PassiveCycler acts as a dispatcher that cycles through all connected passive components on each iteration:

```cpp
// Main loop continuously cycles the PassiveCycler
// which invokes all connected components' activeIn ports
while (true) {
    passiveCycler.cycle();  // Calls rtcDriver.activeIn_handler (and other components)
    __WFI();                // Wait-For-Interrupt: sleep until next hardware event
}
```

**Main Loop Execution:**
1. `passiveCycler.cycle()` invokes all connected `activeIn` ports in sequence
2. Each component (including RtcDriver) checks for pending work (interrupts, events, etc.)
3. `__WFI()` puts the CPU into low-power sleep mode until the next hardware interrupt
4. RTC interrupt wakes the CPU and sets the `wakeup_interrupt` flag
5. Loop repeats: PassiveCycler polls again, RtcDriver detects the flag and emits signals

This pattern enables efficient power management: the CPU sleeps between RTC ticks and wakes only when hardware events occur, while still allowing F´ components to process interrupts through standard port interfaces.

#### 4.4.2 activeIn_handler Execution Flow

Each time the PassiveCycler calls `activeIn_handler`:

1. **Watchdog Reset**: Set `deadline_reached = true` (signals ISR that previous cycle completed)
2. **Interrupt Check**: Check if `wakeup_interrupt` flag is set (indicates RTC interrupt occurred)
3. **Signal Emission** (if interrupt occurred):
   - Acknowledge interrupt by clearing `wakeup_interrupt = false`
   - Capture current timestamp: `Os::RawTime::now()` (combines tick counter + hardware counter)
   - Emit `CycleOverrun` telemetry: `tlmWrite_CycleOverrun(deadline_exceeded)` (reports overrun count)
   - Emit cycle signal: `CycleOut_out(0, cycle_start)` (triggers downstream rate groups)
4. **Return**: Return to PassiveCycler (no-op if no interrupt occurred)

**Interrupt vs Port Handler Responsibilities:**

| Responsibility                          | RTC ISR                       | activeIn_handler                |
| --------------------------------------- | ----------------------------- | ------------------------------- |
| Latency requirement                     | Minimal (< 10 µs)             | Full cycle budget (e.g., 125 ms @ 8Hz) |
| Interrupt flag management               | Set `wakeup_interrupt = true` | Clear `wakeup_interrupt = false` |
| Watchdog check                          | Check `deadline_reached`      | Set `deadline_reached = true`   |
| F´ port invocations                     | None                          | `CycleOut_out()`, `tlmWrite_*()`|
| Timestamp capture                       | None                          | `Os::RawTime::now()`            |
| Overrun detection                       | Increment `deadline_exceeded` | Report via telemetry            |

This separation keeps ISR overhead minimal while allowing full F´ functionality (timestamps, telemetry, port calls) in the port handler.

### 4.5 Topology Connections

```fpp
instance passiveCycler: Svc.PassiveCycler base id 0xD00
instance rtcDriver: Samd21.RtcDriver base id 0xE00
instance rateGroupDriver: Svc.RateGroupDriver base id 0xE10

connections RtcTiming {
  passiveCycler.cycleOut -> rtcDriver.activeIn
  rtcDriver.CycleOut -> rateGroupDriver.CycleIn
}
```

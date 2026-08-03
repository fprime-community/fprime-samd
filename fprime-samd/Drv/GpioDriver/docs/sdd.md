# Samd21::GpioDriver

Driver for the PORT (GPIO) SAMD21 Peripheral.

## Introduction

`GpioDriver` is a passive component that drives a single SAMD21 GPIO pin. It
implements the standard F´ `Drv.Gpio` interface (`gpioRead` / `gpioWrite`), so
it is a drop-in provider for any component that consumes a GPIO port. Each
instance is bound to one pin, selected at configuration time. Reads and writes
are synchronous and go directly to the PORT peripheral registers through a thin
hardware abstraction layer (`GpioHardware::GpioHal`).

> Note: the interface's `gpioInterrupt` output port is not implemented and is
> never invoked.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| GPIO-001 | `configure` shall bind the instance to one group/pin in the requested I/O mode and forward the configuration to the PORT peripheral. | An instance controls exactly one pin. | UT `testConfigureOutput`, `testConfigureInput`, `testConfigureAllPins` |
| GPIO-002 | For input mode, `configure` shall select a pull-up, pull-down, or no internal resistor via `InputPullMode`. | Floating inputs must be pull-able for reliable sensing. | UT `testConfigureInput` |
| GPIO-003 | `gpioWrite` shall set the pin logic level and return `OP_OK` when the pin is configured as an output. | Nominal output path. | UT `testWriteNominal` |
| GPIO-004 | `gpioRead` shall return the pin logic level and `OP_OK` when the pin is configured as an input. | Nominal input path. | UT `testReadNominal` |
| GPIO-005 | `gpioRead` / `gpioWrite` shall return `NOT_OPENED` if invoked before `configure`. | Reject use of an unconfigured pin. | UT `testReadUnconfigured`, `testWriteUnconfigured` |
| GPIO-006 | `gpioRead` on an output pin, or `gpioWrite` on an input pin, shall return `INVALID_MODE` without touching hardware. | Enforce the pin's configured direction. | UT `testReadWrongMode`, `testWriteWrongMode` |

## Design

### Ports

Inherited from the `Drv.Gpio` interface:

| Port | Kind | Purpose |
|---|---|---|
| `gpioRead` | sync input | Read the logic level of the configured input pin. |
| `gpioWrite` | sync input | Write a logic level to the configured output pin. |
| `gpioInterrupt` | output | Declared by the interface but **not implemented**. |

### State

The component stores its configuration in member state: a `m_configured`
flag, the pin `m_group` / `m_pin`, and the I/O `m_mode`. `configure` may be
called only once per instance (guarded by `FW_ASSERT(!m_configured)`); it
records the state and delegates register setup to `GpioHal::configure`.

The handlers are guard-then-delegate:

1. If `!m_configured` return `NOT_OPENED`.
2. If the requested operation does not match `m_mode` return `INVALID_MODE`.
3. Otherwise call `GpioHal::read` / `GpioHal::write` and return `OP_OK`.

### Hardware Abstraction Layer

All register access is isolated behind `GpioHardware::GpioHal`
(`configure` / `read` / `write`). The SAMD21 implementation
(`GpioDriverHardware.cpp`) manipulates the PORT `DIRSET`/`DIRCLR`,
`OUTSET`/`OUTCLR`, `IN`, and `PINCFG` registers. A stub implementation
(`GpioDriverHardwareStub.cpp`) is compiled for native/test builds and records
interactions so the driver can be unit-tested off-target. Selection is made in
`CMakeLists.txt` based on `FPRIME_PLATFORM`.

## Configuration

Call `configure(group, pin, mode, input_pull_mode)` exactly once after
construction and before any port invocation:

- `group` — `Group::PA` or `Group::PB`.
- `pin` — `Pin::PIN_0` .. `Pin::PIN_31`.
- `mode` — `Mode::INPUT` or `Mode::OUTPUT`.
- `input_pull_mode` — `InputPullMode::NO_PULL`, `PULL_DOWN`, or `PULL_UP`.
  Selects the internal pull resistor for input pins; ignored for outputs.

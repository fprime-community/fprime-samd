# Samd21::GpioDriver

Driver for the PORT (GPIO) SAMD21 Peripheral.

## Introduction

`GpioDriver` is a passive component that drives a single SAMD21 GPIO pin. It
implements the standard F´ `Drv.Gpio` interface (`gpioRead` / `gpioWrite`), so
it is a drop-in provider for any component that consumes a GPIO port. Each
instance is bound to one pin and one direction, selected at configuration time
via either `configureInput` or `configureOutput`. Reads and writes are
synchronous and go directly to the PORT peripheral registers through a thin
hardware abstraction layer (`GpioHardware::GpioHal`).

An input pin may optionally be configured to generate edge-triggered external
interrupts through the SAMD21 External Interrupt Controller (EIC). When an
`ExternalInterruptMode` other than `NONE` is selected, the driver enables the
EIC for the pin's `EXTINT` line and, on each configured edge, emits a cycle on
the `gpioInterrupt` output port. The instance therefore acts as an
interrupt-driven `Svc.Cycle` source.

## Requirements

| Name     | Description                                                                                                                          | Rationale                                               | Validation                                                             |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------- | ---------------------------------------------------------------------- |
| GPIO-001 | `configureInput` / `configureOutput` shall bind the instance to one group/pin in the corresponding I/O direction and forward the configuration to the PORT peripheral. | An instance controls exactly one pin.                   | UT `testConfigureOutput`, `testConfigureInput`, `testConfigureAllPins` |
| GPIO-002 | `configureInput` shall select a pull-up, pull-down, or no internal resistor via `InputPullMode`.                                    | Floating inputs must be pull-able for reliable sensing. | UT `testConfigureInput`                                                |
| GPIO-003 | `gpioWrite` shall set the pin logic level and return `OP_OK` when the pin is configured as an output.                                | Nominal output path.                                    | UT `testWriteNominal`                                                  |
| GPIO-004 | `gpioRead` shall return the pin logic level and `OP_OK` when the pin is configured as an input.                                      | Nominal input path.                                     | UT `testReadNominal`                                                   |
| GPIO-005 | `gpioRead` / `gpioWrite` shall return `NOT_OPENED` if invoked before `configureInput` / `configureOutput`.                          | Reject use of an unconfigured pin.                      | UT `testReadUnconfigured`, `testWriteUnconfigured`                     |
| GPIO-006 | `gpioRead` on an output pin, or `gpioWrite` on an input pin, shall return `INVALID_MODE` without touching hardware.                  | Enforce the pin's configured direction.                 | UT `testReadWrongMode`, `testWriteWrongMode`                           |
| GPIO-007 | `configureInput` shall, when `ExternalInterruptMode` is not `NONE`, configure the EIC to detect the selected edge(s) on the pin's `EXTINT` line. | Support edge-triggered input notification.              | Hardware test (Curiosity Nano, PA23)                                   |
| GPIO-008 | On each configured edge, the driver shall emit a cycle on the `gpioInterrupt` output port when it is connected.                     | Notify the consumer of the pin transition.              | Hardware test (Curiosity Nano, PA23)                                   |
| GPIO-009 | `gpioRead`, `gpioWrite`, and interrupt dispatch shall be non-blocking.                                                              | Callers may invoke them from timing-sensitive or interrupt contexts. | Code inspection                                            |

## Design

### Ports

![GpioDriver block diagram showing the Drv.Gpio ports](GpioDriver.svg)

Inherited from the `Drv.Gpio` interface:

| Port            | Kind       | Purpose                                            |
| --------------- | ---------- | -------------------------------------------------- |
| `gpioRead`      | sync input | Read the logic level of the configured input pin.  |
| `gpioWrite`     | sync input | Write a logic level to the configured output pin.  |
| `gpioInterrupt` | output     | Emits a cycle when the configured input pin sees an edge (EIC-driven). |

### State

The component stores its configuration in member state: a `m_configured`
flag, the pin `m_group` / `m_pin`, and the I/O `m_mode` (an internal enum set
implicitly by which configure method was called). Exactly one of
`configureInput` / `configureOutput` may be called per instance (guarded by
`FW_ASSERT(!m_configured)`); it records the state and delegates register setup
to `GpioHal::configureInput` / `GpioHal::configureOutput`.

The handlers are guard-then-delegate:

1. If `!m_configured` return `NOT_OPENED`.
2. If the requested operation does not match `m_mode` return `INVALID_MODE`.
3. Otherwise call `GpioHal::read` / `GpioHal::write` and return `OP_OK`.

## Configuration

Call exactly one of `configureInput` / `configureOutput` once after construction
and before any port invocation. The chosen method fixes the pin direction, so
there is no separate `mode` argument.

**`configureInput(group, pin, input_pull_mode, interrupt_mode)`**

- `group` — `Group::PA` or `Group::PB`.
- `pin` — `Pin::PIN_0` .. `Pin::PIN_31`.
- `input_pull_mode` — `InputPullMode::NO_PULL`, `PULL_DOWN`, or `PULL_UP`.
  Selects the internal pull resistor for the input pin.
- `interrupt_mode` — `ExternalInterruptMode::NONE`, `RISING`, `FALLING`, or
  `BOTH`. Selects EIC edge detection; `NONE` leaves the EIC untouched. When set,
  connect `gpioInterrupt` to receive a cycle on each edge.

**`configureOutput(group, pin)`**

- `group` — `Group::PA` or `Group::PB`.
- `pin` — `Pin::PIN_0` .. `Pin::PIN_31`.

Example (from `Breadboard_Curiosity` topology, `startTasks` phase):

```cpp
inPA23.configureInput(Samd21::GpioDriver::Group::PA,
                      Samd21::GpioDriver::Pin::PIN_23,
                      Samd21::GpioDriver::InputPullMode::PULL_UP,
                      Samd21::GpioDriver::ExternalInterruptMode::BOTH);
outPA25.configureOutput(Samd21::GpioDriver::Group::PA,
                        Samd21::GpioDriver::Pin::PIN_25);
```

`inPA23.gpioInterrupt` is connected to `pinIn.transitionIn`, so pressing the
button on PA23 drives a cycle into `pinIn` on every edge.

## Tested Configurations

| Board Name               | Chip       | Instances                  | Pins            | Ops tested          | Result |
| ------------------------ | ---------- | --------------------------- | --------------- | ------------------- | ------ |
| Microchip Curiosity Nano | SAMD21G17A | `inPA23`, `outPA25`          | PA23 (in), PA25 (out) | configure, gpioRead, gpioWrite, gpioInterrupt | Pass   |

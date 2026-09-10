# Samd21::AdcDriver

A driver for the SAMD21 Analog to Digital Converter Peripheral

## 1. Introduction

The `Samd21::AdcDriver` component drives the SAMD21's single ADC peripheral. It
is a passive, singleton component: exactly one instance may be configured per
deployment, since the SAMD21 has only one ADC and the driver routes the
`ADC_Handler` ISR to whichever instance configured it.

The driver exposes an array of `readAdc` request ports, one per logical
"channel" a client wants to sample. Each port index is bound to a physical
SAMD21 ADC input (`AIN0`-`AIN19`, or an internal source such as the temperature
sensor) via `configureChannel`. Only one conversion may be in flight across the
*entire* peripheral at a time — the ADC itself is a single shared resource,
regardless of how many ports are configured.

Conversion completion is detected in interrupt context (`handleInterrupt`,
invoked from `ADC_Handler`), but the result is not delivered from the ISR.
The ISR records the result and moves the driver to a `COMPLETE`/`COMPLETE_OVERRUN`
state; the `adcResult` callback is delivered later from the `activeIn` tick,
which runs in the main context off an active-component cycler (see §3.4).

## 2. Requirements

| Name           | Description                                                                                                                    | Validation    |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------- | ------------- |
| SAMD21-ADC-001 | The AdcDriver shall configure the ADC peripheral with configurable voltage reference, resolution, hardware averaging, sampling time, and gain. | Hardware Test |
| SAMD21-ADC-002 | The AdcDriver shall bind a `readAdc` port index to a physical ADC input channel (`AIN0`-`AIN19`, `TEMP`, `BANDGAP`, `SCALEDIOVCC`) via `configureChannel`. | Unit Test     |
| SAMD21-ADC-003 | The AdcDriver shall start an asynchronous conversion on `readAdc` and return `ADC_BUSY` if a conversion is already in progress on any port. | Unit Test     |
| SAMD21-ADC-004 | The AdcDriver shall return `ADC_NOT_CONFIGURED` or `ADC_INVALID_CHANNEL` from `readAdc` without touching hardware if the peripheral or the requested port is not configured. | Unit Test     |
| SAMD21-ADC-005 | The AdcDriver shall report an overrun (`ADC_OVERRUN`) if the result register was not read before the next conversion completed. | Unit Test     |
| SAMD21-ADC-006 | The AdcDriver shall deliver every conversion result from the main context (via `activeIn`), not from interrupt context.        | Unit Test     |
| SAMD21-ADC-007 | The AdcDriver shall ignore spurious interrupts (no conversion pending, or flags set with nothing requested) without corrupting driver state. | Unit Test     |

## 3. Design

### 3.1 Overview

`Samd21::AdcDriver` presents an array of `readAdc`/`adcResult` port pairs sized
by `Samd21.ADC_CHANNEL_COUNT` (`Samd21AdcConfig.fpp`), so several client
components can each own a port index without needing to know about the others.
Each port index maps to one physical ADC input, bound once via
`configureChannel`. Because the SAMD21 has a single ADC conversion engine, the
driver still only allows one conversion in flight at a time regardless of
how many ports are configured — a `readAdc` request while another conversion is
outstanding is rejected immediately with `ADC_BUSY`.

The conversion outcome is detected in interrupt context
(`handleInterrupt`, called from the `ADC_Handler` ISR trampoline in
`AdcDriverIsr.cpp`), but the client callback is not invoked from there.
The ISR stores the result and moves the driver into a terminal `COMPLETE` or
`COMPLETE_OVERRUN` state; the `adcResult` callback is delivered later from the
`activeIn` tick, which runs in the main context off an active-component cycler.
This means **the result callback runs in the main loop, not ISR context**, so
the client handler is free to do ordinary work (telemetry, events, starting the
next request). `activeIn` must therefore be connected for the driver to
function.

### 3.2 Ports

| Kind         | Name                   | Port Type          | Usage                                                      |
| ------------ | ----------------------- | ------------------- | ----------------------------------------------------------- |
| `sync input` | `readAdc[ADC_CHANNEL_COUNT]`  | `Samd21.AdcRead`    | Start a conversion on the port's bound channel               |
| `output`     | `adcResult[ADC_CHANNEL_COUNT]` | `Samd21.AdcResult`  | Conversion result + status (from main context)               |
| `sync input` | `activeIn`              | `Svc.ActiveSched`   | Main-context tick that delivers a pending result             |

`readAdc` returns `AdcStatus` synchronously: `ADC_OK` once the conversion has
been *started* (the value itself is not yet available), or an immediate
rejection status (`ADC_BUSY`, `ADC_NOT_CONFIGURED`, `ADC_INVALID_CHANNEL`).

`activeIn` returns `bool`: `true` when a result was delivered this tick, `false`
when there was nothing pending — matching the `I2cDriver`/`UsartDriver`
`activeIn` convention. Being `Svc.ActiveSched` rather than a passive
`Svc.Sched` rate-group tick, `activeIn` is driven directly from a cycler (see
§4.2), not from a `Svc.PassiveRateGroup` member port.

The `readAdc`/`adcResult` ports are arrayed and indexed identically (index *n*
of `readAdc` always replies on index *n* of `adcResult`), sized by the
`Samd21.ADC_CHANNEL_COUNT` compile-time constant.

### 3.3 Configuration

#### 3.3.1 Runtime (`configure()` / `configureChannel()`)

`configure()` sets up the ADC peripheral once at startup. The caller selects:

| Option           | Choices                                                                 |
| ----------------- | ------------------------------------------------------------------------ |
| Voltage reference | `INT1V`, `INTVCC0`, `INTVCC1`, `VREFA`, `VREFB`                          |
| Resolution        | 8-bit, 10-bit, or 12-bit                                                 |
| Hardware averaging | 1 to 1024 samples (`SampleCount::SAMPLES_1` … `SAMPLES_1024`)            |
| Sampling time     | `SAMPCTRL.SAMPLEN` value, 0-63                                           |
| Gain              | 1x, or 0.5x (`GAIN_DIV2`, extends input range to 2x the reference)       |

`configure()` asserts it has not already been called (`s_instance == nullptr`),
delegates the full hardware init sequence (clock gating, calibration load,
reference/resolution/averaging setup, dummy conversion per the SAMD21 datasheet)
to the HAL, registers the singleton for `ADC_Handler` to route to, and only then
enables the ADC and NVIC interrupts.

`configureChannel(portNum, channel)` binds one `readAdc` port index to a
physical ADC input. It asserts the peripheral is configured, `portNum` is in
range, and the port has not already been bound (no re-binding). For the two
channels that require enabling an internal SYSCTRL resource (`TEMP`,
`BANDGAP`), it delegates that one-time setup to the HAL; `SCALEDIOVCC` and the
external `AIN0`-`AIN19` channels need no such setup — external channels do need
their pin muxed via `Samd21::PinMux::configure()` in the topology first, since
that is unrelated to the ADC peripheral itself.

#### 3.3.2 Compile-time

| Setting                     | Where                        | Default | Effect                                                                 |
| ---------------------------- | ------------------------------ | ------- | ------------------------------------------------------------------------ |
| `Samd21.ADC_CHANNEL_COUNT`   | `Samd21AdcConfig.fpp`          | 10      | Number of `readAdc`/`adcResult` port pairs (how many channels can be bound) |

> A deployment may override this in its own `samd-config`. `ADC_CHANNEL_COUNT`
> trades RAM (roughly 2 bytes per port for the channel-mapping arrays) against
> how many logical channels can be bound; it need not equal 20 (the number of
> physical `AIN` pins) since a deployment typically only uses a handful.

### 3.4 State Machine

The driver tracks conversion progress in a single `State` (member `m_state`),
shared across *all* ports since the ADC is one physical resource:

| State              | Meaning                                                              |
| -------------------- | ----------------------------------------------------------------------- |
| `IDLE`               | No conversion in progress; a new `readAdc` request is accepted.       |
| `CONVERTING`         | A conversion is in flight; waiting on the ADC interrupt.              |
| `COMPLETE`           | Conversion finished successfully; awaiting `activeIn` delivery.       |
| `COMPLETE_OVERRUN`   | Conversion finished but an overrun was also detected; awaiting delivery. |

Nominal flow: `readAdc` moves `IDLE -> CONVERTING`, selects the port's bound
channel and gain, and starts the conversion. `handleInterrupt` (ISR) reads the
result, moves `CONVERTING -> COMPLETE` (or `COMPLETE_OVERRUN` if an overrun was
also flagged), and returns. The next `activeIn` tick delivers the result on the
requesting port's `adcResult` output and moves `COMPLETE* -> IDLE`, at which
point a new `readAdc` (on any port) may proceed.

This is a deliberately **lock-free** design: unlike `I2cDriver`/`UsartDriver`,
no `CriticalSection` guards the fields shared between main context
(`readAdc_handler`/`activeIn_handler`) and the ISR (`handleInterrupt`). This is
safe only because each state transition above has exactly one writer, and that
writer only ever acts on state left by the *other* side — the two contexts can
never observe or mutate the same field at the same time. See the comment above
`AdcDriver::State` in `AdcDriver.hpp` for the full invariant; any future change
that adds a third reader/writer (e.g. a timeout/abort path) must re-verify it or
add a `CriticalSection`.

### 3.5 Error Handling

`handleInterrupt` treats an interrupt that fires while `m_state != CONVERTING`
as spurious: it clears whatever flags are set (`OVERRUN`, `RESRDY`) without
changing driver state, so a stray or duplicate interrupt cannot corrupt an
in-flight or already-completed conversion.

An overrun (`INTFLAG.OVERRUN`, meaning a new result landed before the previous
one was read) is detected in the same ISR pass as the result: the driver still
reads and delivers the result, but reports `ADC_OVERRUN` instead of `ADC_OK` so
the client knows the value's timing is suspect.

`readAdc` rejects requests up front — `ADC_NOT_CONFIGURED` if `configure()`
hasn't run, `ADC_INVALID_CHANNEL` if the port hasn't been bound via
`configureChannel`, `ADC_BUSY` if another conversion is in flight — without
touching any hardware register in the rejection paths.

### 3.6 Telemetry, Events, and Commands

None. The driver reports conversion outcomes solely through the `adcResult`
status field (`ADC_OK` / `ADC_OVERRUN`); it emits no events or telemetry
channels and accepts no commands.

## 4. Integration

### 4.1 Initialization

For external channels (`AIN0`-`AIN19`), configure the pin muxing via
`Samd21::PinMux::configure()` **before** calling `configureChannel()` for that
port — the ADC HAL does not touch pin muxing itself. `configure()` must be
called exactly once, before any `configureChannel()` or `readAdc` call.

### 4.2 Topology Connections

`activeIn` is `Svc.ActiveSched`, not a passive rate-group tick, so it connects
directly to a cycler's `cycleOut` (alongside other active components) rather
than to a `Svc.PassiveRateGroup` member port. If `activeIn` is left
unconnected, results are queued in `COMPLETE`/`COMPLETE_OVERRUN` and never
delivered, and the peripheral is permanently stuck busy after the first
conversion.

Each client component connects its own `readAdc`/`adcResult` port pair at the
index it was bound to via `configureChannel`. From `CastleTester`:

```fpp
instance adcDriver: Samd21.AdcDriver base id 0xBB90 \
{
  phase Fpp.ToCpp.Phases.configComponents """
    Samd21::PinMux::configure(PINMUX_PB02B_ADC_AIN10);  // PB02 -> ADC AIN10 (function B)

    adcDriver.configure(
      Samd21::AdcDriver::VoltageReference::INTVCC1,  // VDDANA/2 reference (~1.65V)
      Samd21::AdcDriver::Resolution::RES_12BIT,
      Samd21::AdcDriver::SampleCount::SAMPLES_16,    // 16x averaging for noise reduction
      3,                                              // Sampling time
      Samd21::AdcDriver::Gain::GAIN_1X
    );

    adcDriver.configureChannel(0, Samd21::AdcDriver::AdcChannel::AIN10);
  """
}

connections ActiveRateGroups {
  cycler.cycleOut -> adcDriver.activeIn
}

connections PowerAdc {
  # Power component requests ADC conversion
  power.adcRead -> adcDriver.readAdc[0]
  # ADC driver delivers result back to Power component
  adcDriver.adcResult[0] -> power.adcResult
}
```

**Wiring requirements:**
- `activeIn` must be connected to a cycler (not a passive rate group) or
  results are never delivered.
- Every `readAdc`/`adcResult` port index used by a client must first be bound
  with `configureChannel` at the matching index.

## 5. Tested Configurations

| Board Name               | Chip       | Channel | Reference | Resolution | Gain | Ops tested         | Result |
| ------------------------- | ---------- | ------- | --------- | ---------- | ---- | -------------------- | ------ |
| Microchip Curiosity Nano | SAMD21G17A | AIN10   | INTVCC1   | 12-bit     | 1x   | configure, readAdc, activeIn | Pass   |

Verified by driving AIN10 from a resistive voltage divider and confirming the
delivered ADC codes tracked the divider voltage as it was adjusted (not checked
against a calibrated reference).

## 6. Limitations

- One conversion in flight at a time across the *entire* peripheral, regardless
  of how many `readAdc` ports are configured; concurrent requests are rejected
  with `ADC_BUSY`.
- Result delivery runs in the **main context** (via `activeIn`), so `activeIn`
  must be connected to a cycler; a result is not delivered until the next tick
  after the conversion completes.
- Configuration is one-time; no runtime reconfiguration of voltage reference,
  resolution, averaging, or gain, and no re-binding of a port's channel.
- Singleton: only one `AdcDriver` instance may be configured per deployment
  (the SAMD21 has one ADC peripheral and one `ADC_Handler` vector).
- No telemetry, events, or commands — conversion status is reported only
  through the `adcResult` port.

# Svc::PassiveCycler

A passive component that drives the main-context "active" schedule of a deployment without
threads: `cycle()` calls every consumer wired to `cycleOut` until none of them reports work
left to do, and `idle()` is one main-loop turn — `cycle()` followed by a sleep that is skipped
if an interrupt was taken during the cycle.

## 1. Introduction

On the SAMD21 there is no scheduler and there are no threads. The deployment's main loop is
`cycler.cycle(); asm("wfi");` (`fprime-samd-reference` `CuriosityReference/Main.cpp`): service
everything that has work, then sleep until the next interrupt. Driver ISRs only latch state;
the components that turn latched state into port calls (DMA and I2C completions, SPI replies,
rate-group ticks, sequence steps) do so from their `Svc.ActiveSched` input, called from here.
The port's `bool` return is the consumer's way to say "call me again before sleeping".

Because consumers only act when called, an interrupt that lands *after* its consumer's turn in
a pass but *before* the `wfi` is lost to that turn: the ISR has already run when the MCU
sleeps, so nothing wakes it until the next unrelated interrupt (the 8 Hz RTC tick in the
reference deployment, up to 125 ms). The effect was measured on `Samd21.SpiDriver` with a
client that starts the next frame from its completion callback: one frame per tick, a 256 KiB
F-RAM sweep in 458 s. `idle()` closes that window for every consumer at once (§3.2).

## 2. Requirements

| Name | Description | Rationale | Validation |
| ---- | ----------- | --------- | ---------- |
| SAMD21-CYC-001 | On every pass `cycle()` shall call every connected `cycleOut` port once, in index order, with `context` equal to the port index, regardless of what earlier ports in the pass returned. | A consumer reporting work must not starve the consumers after it; order is the deployment's documented service order. | Unit Test |
| SAMD21-CYC-002 | `cycle()` shall repeat the pass while any consumer returned `true` in the previous pass and return once a whole pass returned `false` everywhere. | Work produced during a pass (a completion delivered to a client that starts the next one) is serviced before the MCU sleeps. | Unit Test |
| SAMD21-CYC-003 | Unconnected `cycleOut` ports shall be skipped. | Deployments wire fewer consumers than `PassiveRateGroupOutputPorts`. | Inspection |
| SAMD21-CYC-004 | `idle()` shall run `cycle()` and then sleep until the next interrupt when no interrupt was taken between the start of the cycle and the sleep decision; interrupts taken before the cycle started shall not prevent the sleep. | The cycle services everything latched before it started; sleeping is only wrong if something was latched during it. | Unit Test |
| SAMD21-CYC-005 | `idle()` shall not sleep when an interrupt was taken during the cycle, so that the caller's loop cycles again; the check and the sleep shall be atomic with respect to interrupts. | An ISR that latches work after its consumer's turn must cause one more pass, not a sleep until the next tick; an interrupt between the check and the WFI must still wake the MCU. | Unit Test (decision), Hardware (WFI wake-up) |

## 3. Design

### 3.1 cycle()

```
cycle():
    didWork = true
    while didWork:
        didWork = false
        for i in 0 .. NUM_CYCLEOUT_OUTPUT_PORTS-1:
            if connected(i): didWork = cycleOut_out(i, i) || didWork
```

The port call is the left operand of the `||` on purpose: `didWork || cycleOut_out(i, i)`
would short-circuit once one consumer had returned `true` and skip every consumer after it for
the rest of that pass, delaying them by a pass each time an earlier consumer reports work (a
consumer that reports work for the whole of an SPI burst would starve them for the burst).

The component has no input ports, events or telemetry; `cycle()`/`idle()` are called directly
by `main`. It is bounded by its consumers: `cycle()` returns only when a pass is quiet, so a
consumer that answers `true` unconditionally keeps main awake (and every other consumer
serviced).

### 3.2 idle()

```
idle():
    Samd21::Idle::arm()                    // samd21_irq_taken = 0
    cycle()
    Samd21::Idle::sleepUnlessInterrupted() // __disable_irq(); if (!samd21_irq_taken) WFI; __enable_irq()
```

**How "work pending" is known.** Every entry of the SAMD21 vector table (`Mcu/reset.c`) is a
trampoline that sets `samd21_irq_taken = 1` and then calls the driver's `XXX_Handler` (the
weak-alias scheme is unchanged, so drivers still define the plain handler symbols). The signal
is therefore "an interrupt was taken", not "a component has work": it costs one store and one
call per interrupt (≈ 8 cycles, 29 trampolines ≈ 350 bytes of flash) and asks nothing of the
drivers — no driver can forget to signal, and a new driver is covered on day one. The price is
a spurious extra pass whenever an interrupt that produced no main-context work landed during a
cycle (every consumer answers `false`, ≈ the cost of one quiet pass). The alternative, an
ISR-settable flag that each latching ISR must raise, is cheaper per interrupt but makes
correctness depend on every present and future ISR doing so; the measured failure already came
from exactly that kind of per-driver contract.

`arm()` is a plain store: an interrupt that sets the flag just before the store is serviced by
the `cycle()` that follows, so losing it is harmless, and one that sets it afterwards is seen.
`sleepUnlessInterrupted()` masks interrupts (PRIMASK) around the check and the `WFI`: an
interrupt arriving between them stays pending, and a pending interrupt ends `WFI` whether or not
PRIMASK is set (ARMv6-M ARM, B1.5.17 "Wait For Interrupt"); the ISR then runs after
`__enable_irq()`, sets the flag again, and the caller's next `idle()` services it. The sleep
depth is whatever `PM->SLEEP` / `SCR.SLEEPDEEP` the deployment configured; `idle()` does not
change it. Preconditions: main context, interrupts enabled, no `Samd21::CriticalSection` open
(`Idle` uses the raw PRIMASK intrinsics, not the nesting counter).

`Samd21::Idle` lives in `Drv/Types` next to `CriticalSection`, selected per platform like it:
`Idle.cpp` for the SAMD21 target, `test/ut/IdleStub.cpp` for native builds. The stub makes the
same decision on a simulated flag and counts arms/sleeps/skips so the cycler's unit test can
drive it; only the SAMD21 (Cortex-M0+) table is instrumented — the legacy SAMD51 table in
`reset.c` has no toolchain in this library and is untouched.

## 4. Integration

Instantiate once, connect `cycler.cycleOut[i] -> consumer.activeIn` in the order the
consumers should be serviced within a pass, and make the main loop

```cpp
for (;;) {
    CuriosityReference::cycler.idle();   // replaces: cycler.cycle(); asm("wfi");
}
```

A loop that keeps calling `cycle()` and its own `wfi` still works but keeps the latency window
described in §1. A consumer that delivers callbacks to other consumers (e.g. `Samd21.SpiDriver`
to its clients) is normally wired before them so the client sees the completion in the same
pass; this is a latency preference, not a correctness requirement, since every consumer is
called on every pass.

## 5. Unit tests

`test/ut/` (7 tests, registered by the change that fixed the pass; there were none before)
wires all `PassiveRateGroupOutputPorts` consumers to a tester that counts calls and answers
`true` a configurable number of times per port. `cycle()`: nobody has work (one pass, each
consumer once, in order, `context` = index), work reported by the first consumer (the later
consumers are still called in that pass, then a full second pass), work reported by the last
consumer (two full passes), repeated work from two consumers (n + 1 full passes, nobody
skipped). `idle()` against the `Idle` stub: a quiet turn arms once and sleeps (also when a
consumer added passes), an interrupt simulated while the last or the first consumer runs skips
the sleep and the next turn services it and sleeps, and an interrupt taken before the turn does
not prevent the sleep. The `cycle()` tests fail against the previous `didWork || cycleOut_out`
expression (EarlyTrue, RepeatedWork), which is the defect they guard.

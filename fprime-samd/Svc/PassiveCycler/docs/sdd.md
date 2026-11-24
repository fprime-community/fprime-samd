# Svc::PassiveCycler

Runs the main-context work of passive components until none of them has work left.

## 1. Introduction

On a baremetal deployment there are no threads, so components that need to finish work outside an
interrupt (DMA completions, I2C/SPI transactions, sequence steps, parameter transfers) expose an
`activeIn` port of type `Svc.ActiveSched`. `PassiveCycler` drives those ports from the main loop.
The deployment calls `cycle()` after every wake-up, and `cycle()` returns once every consumer
reports it has nothing more to do, so the CPU can go back to sleep.

## 2. Requirements

| Name                     | Description                                                                                                                         | Validation |
| ------------------------ | ----------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| SAMD21-PASSIVECYCLER-001 | `cycle()` shall call each connected `cycleOut` port once per pass, in ascending port order, passing the port index as the context. | Unit Test  |
| SAMD21-PASSIVECYCLER-002 | `cycle()` shall repeat passes until a pass in which no consumer returns `true`, then return.                                        | Unit Test  |
| SAMD21-PASSIVECYCLER-003 | Every connected consumer shall be called on every pass, including after an earlier consumer in the same pass returned `true`.      | Unit Test  |
| SAMD21-PASSIVECYCLER-004 | `cycle()` shall skip unconnected `cycleOut` ports, and shall return without calling anything when no port is connected.            | Unit Test  |

## 3. Design

`cycle()` is a public method, not a port, so the deployment's `main()` can call it directly. Each
pass walks `cycleOut` from index 0 up. A consumer returns `true` when it did work and may have more,
which schedules another pass over all consumers. Work done by one consumer often creates work for
another (for example, an I2C completion that lets a sensor component start its next read), so a
full pass is repeated rather than only re-calling the consumer that asked.

`cycle()` does not bound the number of passes. A consumer that always returns `true` keeps the CPU
awake and starves the main loop's sleep. Each consumer must therefore return `false` once its
pending work is done.

`cycle()` must be called from main context only, never from an interrupt: consumers run their
completion work there, including port calls that are not interrupt-safe.

## 4. Ports

| Kind     | Name       | Port Type          | Usage                                                         |
| -------- | ---------- | ------------------ | ------------------------------------------------------------- |
| `output` | `cycleOut` | `Svc.ActiveSched`  | Main-context work; connect to each component's `activeIn` port |

`cycleOut` has `PassiveRateGroupOutputPorts` entries.

## 5. Configuration

None.

## 6. Integration

From the FFB deployment. Unindexed connections are assigned to free `cycleOut` ports in the order
the topology lists them, which sets the order consumers are called in each pass:

```fpp
instance cycler: Svc.PassiveCycler base id FixedBaseIds.NO_DICTIONARY

connections Cycler {
    cycler.cycleOut -> uart.activeIn
}

connections ActiveRateGroups {
    cycler.cycleOut -> rateDriver.activeIn
    cycler.cycleOut -> i2cDriver.activeIn
    cycler.cycleOut -> seq.activeIn
    cycler.cycleOut -> adcDriver.activeIn
    cycler.cycleOut -> spiDriver.activeIn
    cycler.cycleOut -> prm.activeIn
}
```

```cpp
for (;;) {
    cycler.cycle();  // Run until every consumer is idle
    asm("wfi");      // Sleep until the next interrupt
}
```

The component has no commands, events, telemetry, or parameters.

## 7. Tested Configurations

| Board                    | Chip       | Consumers                                           |
| ------------------------ | ---------- | --------------------------------------------------- |
| Microchip Curiosity Nano | SAMD21G17A | Breadboard: USART, RTC, I2C, Seq, parameter database |
| FFB                      | SAMD21J17D | USART, RTC, I2C, Seq, ADC, SPI, parameter database  |

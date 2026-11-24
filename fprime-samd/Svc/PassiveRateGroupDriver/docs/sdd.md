# Samd21::PassiveRateGroupDriver

Divides one system tick into the ticks of several `Svc.PassiveRateGroup` components.

## 1. Introduction

`PassiveRateGroupDriver` is the passive counterpart of `Svc.RateGroupDriver`. It receives the system
tick on `CycleIn` (from `Samd21.RtcDriver`) and calls each `CycleOut` port at a fixed fraction of that
rate. Each port's rate is set by a divisor and an offset. The rate groups run synchronously inside
the call, so `CycleIn` returns only after every rate group due on that tick has finished. The
component reports how long that took.

## 2. Requirements

| Name               | Description                                                                                                                                                                                             | Validation |
| ------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| SAMD21-RGDRIVER-001 | On the `n`-th `CycleIn` after `configure()` (counting from 0), `CycleOut[i]` shall be called when `n % divisor[i] == offset[i]`. Calling `configure()` again shall restart the count from 0 with the new table. | Unit Test  |
| SAMD21-RGDRIVER-002 | A `CycleOut` port with divisor 0, or that is not connected, shall never be called.                                                                                                                      | Unit Test  |
| SAMD21-RGDRIVER-003 | The schedule shall continue without skipping or repeating a port's call when the tick count wraps.                                                                                                     | Unit Test  |
| SAMD21-RGDRIVER-004 | Each `CycleOut` call shall carry a timestamp taken no earlier than the `CycleIn` cycle start.                                                                                                          | Unit Test  |
| SAMD21-RGDRIVER-005 | Every `CycleIn` shall report the time from its cycle start to the end of its dispatch on `CycleTime`, and the largest such time since boot on `MaxCycleTime`.                                            | Unit Test  |
| SAMD21-RGDRIVER-006 | The component shall assert on a `CycleIn` before `configure()`, on a divider whose offset is non-zero and not below its divisor, and on a table whose least common multiple of divisors would overflow `FwSizeType`. | Unit Test  |

## 3. Design

`configure()` copies the divider table and sets the rollover to the least common multiple of the
non-zero divisors (`Utils::lcm`, as in `Svc.RateGroupDriver`). Every divisor divides the rollover,
so wrapping the tick count at the rollover keeps every port's phase. A table with no non-zero
divisor has a rollover of 1 and calls nothing. The FFB table `{1, 8, 80}` rolls over at 80.

On each `CycleIn` the ports are checked in ascending order. A port that is due gets a fresh
`Os::RawTime::now()` as its cycle start, so each `Svc.PassiveRateGroup` measures its own execution
time rather than the time since the tick. After the loop the tick count advances. `CycleTime` is
then measured from the `CycleIn` cycle start, so it includes every rate group that ran on the tick.
Both channels are written on every tick.

## 4. Ports

| Kind        | Name       | Port Type   | Usage                                                         |
| ----------- | ---------- | ----------- | ------------------------------------------------------------- |
| `sync input` | `CycleIn`  | `Svc.Cycle` | System tick, from `rateDriver.CycleOut`                       |
| `output`    | `CycleOut` | `Svc.Cycle` | One per rate group; connect to each `PassiveRateGroup.CycleIn` |

`CycleOut` has `RateGroupDriverRateGroupPorts` entries, and the divider table has the same size
(`PassiveRateGroupDriver::DIVIDER_SIZE`).

## 5. Configuration

`configure(const DividerSet&)` takes one `Divider{divisor, offset}` per `CycleOut` port. It must be
called before the first tick, normally from `configureTopology()`.

## 6. Integration

From the FFB deployment, with an 8 Hz `Samd21.RtcDriver`:

```cpp
// 8 Hz, 1 Hz and 0.1 Hz
Samd21::PassiveRateGroupDriver::DividerSet rateGroupDivisors{{{1, 0}, {8, 0}, {80, 0}}};

void configureTopology(const TopologyState& state) {
    rateGroupDriver.configure(rateGroupDivisors);
    // ...
}
```

```fpp
instance rateGroupDriver: Samd21.PassiveRateGroupDriver base id 0x2010

connections RateGroups {
    rateDriver.CycleOut -> rateGroupDriver.CycleIn
    rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup1] -> rg8Hz.CycleIn
    rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup2] -> rg1Hz.CycleIn
    rateGroupDriver.CycleOut[Ports_RateGroups.rateGroup3] -> rg10s.CycleIn
}
```

Spread slow rate groups with offsets (for example `{8, 4}`) when they should not run on the same tick
as another slow group.

## 7. Limitations

- `CycleTime` is a `U32` of microseconds and saturates at about 71 minutes.
- `MaxCycleTime` is never reset, and it includes the first tick after boot.
- Every port due on a tick runs inside the same `CycleIn` call, so a tick where several slow
  groups line up takes the sum of their times. `Samd21.RtcDriver` flags a tick that overruns the
  period. Use offsets to spread slow groups.

## 8. Tested Configurations

| Board                    | Chip       | Table (8 Hz tick) |
| ------------------------ | ---------- | ----------------- |
| Microchip Curiosity Nano | SAMD21G17A | `{1, 8, 80}`      |
| FFB                      | SAMD21J17D | `{1, 8, 80}`      |

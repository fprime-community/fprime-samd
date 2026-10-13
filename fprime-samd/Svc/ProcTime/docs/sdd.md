# Samd21::ProcTime

A passive time component that reports processor time from the SAMD21 realtime clock.

## 1. Introduction

`ProcTime` implements the `Fw.Time` port so a deployment can timestamp events and telemetry
with the time elapsed since boot. The time is in the `TB_PROC_TIME` time base and is not tied
to any external (wall clock) time.

## 2. Requirements

| Name                | Description                                                          | Validation |
| ------------------- | -------------------------------------------------------------------- | ---------- |
| SAMD21-PROCTIME-001 | ProcTime shall report time in the TB_PROC_TIME time base.            | Unit Test  |
| SAMD21-PROCTIME-002 | ProcTime shall report the time elapsed on the Os::RawTime clock.     | Unit Test  |

## 3. Design

On each `getTime` call the component reads `Os::RawTime::now()` and returns its interval from a
default-constructed `Os::RawTime` (the clock's zero point) as an `Fw::Time` with context `0`.

On SAMD21, `Os::RawTime` is compile-time aliased to `Samd21::RawTime` (see
`default/samd-config/OsSelection.hpp`), which is backed by `Samd21.RtcDriver`. The clock
therefore counts from when the RTC was configured, at the RTC driver's tick resolution plus
its sub-tick counter.

## 4. Ports

| Kind         | Name      | Port Type | Usage                    |
| ------------ | --------- | --------- | ------------------------ |
| `sync input` | `getTime` | `Fw.Time` | Returns the current time |

## 5. Integration

Instantiate it as the deployment's time component and connect it to the components' time get
ports:

```fpp
instance timeHandler: Samd21.ProcTime base id 0x2000
```

`Samd21.RtcDriver` must be configured for the time to advance (see Limitations).

## 6. Limitations

- Reports `0` until `Samd21.RtcDriver` has been configured.
- Restarts from `0` on every reset; it is not persistent or synchronized to ground time.
- The RTC tick counter is a `U32`, so the time wraps after 2^32 ticks (about 17 years at 8 Hz,
  about 388 days at 128 Hz).

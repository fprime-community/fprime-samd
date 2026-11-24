# Samd21::SystemInfo

A component for reporting system information about the SAMD21 MCU.

## 1. Introduction

`SystemInfo` reports two kinds of startup/diagnostic information: the cause
of the MCU's most recent reset, and the git commit of each of the three
repositories baked into the flashed image (project, fprime, fprime-samd).
Both are needed to answer "what is actually running, and why did it restart"
from the ground without a debug session. It also provides a `RESET` command
to reset the MCU from the ground.

Reporting is triggered by calling the public `emit()` method directly —
it is not exposed as a command. The reset cause is read fresh from hardware on
every call rather than cached at construction, so it reflects whatever most
recently reset the MCU even if `emit()` is called repeatedly. The commit
stamps are build-time constants burned into a generated header
(`SystemInfoVersion.hpp`, see §8).

## 2. Requirements

| Name           | Description                                                                              | Validation |
| -------------- | ----------------------------------------------------------------------------------------- | ---------- |
| SAMD21-HEALTH-001 | SystemInfo shall report the cause of the most recent reset when emit() is called.    | Unit Test  |
| SAMD21-HEALTH-002 | SystemInfo shall report the build commits of the project, fprime and fprime-samd.     | Unit Test  |
| SAMD21-HEALTH-003 | SystemInfo shall reset the MCU on receipt of the RESET command.                        | Unit Test  |

## 3. Design

`emit()` reads the current reset cause and the build commit stamps, and
reports both as an event and as telemetry.

The `RESET` command responds `OK` and then resets the MCU through the HAL
(`NVIC_SystemReset()`, a system reset request via the Cortex-M0+ `AIRCR`
register). The response is sent before the reset because the reset does not
return; the response is therefore only a best-effort acknowledgement, and it
may not reach the ground if the downlink has not drained before the core
resets. The reset is confirmed instead by the `SystemInfo` event emitted on
the next boot, which reports a `SYSTEM` reset cause (PM `RCAUSE.SYST`).

The reset cause comes from the SAMD21's PM `RCAUSE` register (datasheet
§17.8.14), which latches the reason for the most recent reset (power-on,
brown-out, external, watchdog, or system reset). More than one flag in
`RCAUSE` can be set at once, so the component decodes it in a fixed priority
order and reports a single cause. Hardware access is isolated behind a HAL
with a stub implementation for Linux/UT builds, so this decode is only
exercised on real hardware.

## 4. Ports

| Kind         | Name        | Port Type | Usage                                |
| ------------ | ----------- | --------- | ------------------------------------- |
| `time get`   | `timeCaller`| —         | Timestamp source for events/telemetry |

Plus the standard `Fw.Command`, `Fw.Event` and `Fw.Channel` autocoded ports.

## 5. Commands

| Command | Kind | Arguments | Description                                         |
| ------- | ---- | --------- | --------------------------------------------------- |
| `RESET` | sync | —         | Respond `OK`, then reset the MCU. Does not return.  |

## 6. Events

| Event        | Severity      | Arguments                                                     | Description                      |
| ------------ | ------------- | ------------------------------------------------------------- | --------------------------------- |
| `SystemInfo` | activity high | `resetReason`, `projectCommit`, `fprimeCommit`, `samdCommit`  | Reset cause + the three build commits, on `emit()` |

## 7. Telemetry

| Name            | Type                 | Update       | Description                         |
| --------------- | -------------------- | ------------ | ------------------------------------ |
| `ResetCause`    | `Samd21.ResetReason` | On `emit()`  | Cause of the most recent reset       |
| `ProjectCommit` | `U64` (hex)          | On `emit()`  | Leading 64 bits of the project HEAD  |
| `FprimeCommit`  | `U64` (hex)          | On `emit()`  | Leading 64 bits of the fprime HEAD   |
| `SamdCommit`    | `U64` (hex)          | On `emit()`  | Leading 64 bits of the fprime-samd HEAD |

These are deliberately four channels rather than one struct-valued channel.
A struct carrying all four fields serializes to 28 bytes (a 4-byte
`FwEnumStoreType` plus three `U64`s), which exceeds the project's
`FW_TLM_BUFFER_MAX_SIZE` of 8 (`config-moonfallpm/FpConstants.fpp`) — writing
it would fail serialization and trip `FW_ASSERT` inside the autocoded
`tlmWrite`. Each channel above fits the 8-byte budget exactly, so no
project-wide buffer growth is needed. The event is unaffected because
`FW_LOG_BUFFER_MAX_SIZE` is 120.

## 8. Configuration

No runtime `configure()` — the component is ready to use as soon as it is
constructed. All configuration is build-time:

- **Reset cause decode**: fixed, defined by the PM `RCAUSE` register layout
  (hardware build only).
- **Commit stamps**: generated by `GitVersion.cmake` into
  `SystemInfoVersion.hpp` at configure time, and re-stamped on every build
  (via an `add_custom_target` dependency) so a new commit is picked up
  without re-running `cmake --preset`. A SHA-1 does not fit the `U64` channel
  type, so each commit is truncated to its leading 16 hex digits — a
  superset of any abbreviated hash `git` itself would print, so the value
  stays usable with `git show <hex>`. A missing `git`, or a non-repository
  source tree, reports commit `0` with a configure-time warning rather than
  failing the build.

## 9. Integration

`emit()` is called directly on the instance, typically once at startup from
an `instances.fpp` init-code phase:

```fpp
instance systemInfo: Samd21.SystemInfo base id 0x3050 {
    phase Fpp.ToCpp.Phases.startTasks """
    systemInfo.emit();
    """
}
```

No port connections are required beyond `timeCaller` (wired to the
deployment's time component), the autocoded command ports to the command
dispatcher (covered by a `command connections instance cmdDisp` pattern), and
the autocoded event/telemetry ports to the usual logger/channel components.

## 10. Tested Configurations

| Build                      | Hardware source used          | Reset reasons exercised                                                   |
| --------------------------- | ------------------------------ | --------------------------------------------------------------------------- |
| `native-ut` (Linux)         | `SystemInfoHardwareStub.cpp`  | All of `ResetReason`: `POWER_ON`, `BROWN_OUT_12`, `BROWN_OUT_33`, `EXTERNAL`, `SYSTEM`, `WATCHDOG_TIMER`, `UNKNOWN` |
| `microchip_curiosity` (SAMD21G17A), `ncc` (SAMD21J17D) | `SystemInfoHardware.cpp` | Exercised on both J17 and G17 variants |

## 11. Limitations

- Only reports a single reset cause even though multiple `RCAUSE` flags can
  be set simultaneously; the priority order in §3 determines which one is
  reported, and the others are silently dropped.
- Commit stamps are a single build-time snapshot; if the project, fprime, or
  fprime-samd tree is dirty (uncommitted changes) at build time, the reported
  commit does not reflect those changes.

# Samd21::SystemInfo

A component for monitoring the health and state of the SAMD21 MCU.

## 1. Introduction

`SystemInfo` reports two kinds of startup/diagnostic information on command:
the cause of the MCU's most recent reset, and the git commit of each of the
three repositories baked into the flashed image (project, fprime,
fprime-samd). Both are needed to answer "what is actually running, and why
did it restart" from the ground without a debug session.

The reset cause is read fresh from hardware on every command rather than
cached at construction, so it reflects whatever most recently reset the MCU
even if `EMIT_SYSTEM_INFO` is sent repeatedly. The commit stamps are
build-time constants burned into a generated header (`SystemInfoVersion.hpp`,
see §4).

## 2. Requirements

| Name           | Description                                                                              | Validation |
| -------------- | ----------------------------------------------------------------------------------------- | ---------- |
| SAMD21-HEALTH-001 | SystemInfo shall report the cause of the most recent reset on command.               | Unit Test  |
| SAMD21-HEALTH-002 | SystemInfo shall report the build commits of the project, fprime and fprime-samd.     | Unit Test  |

## 3. Design

`EMIT_SYSTEM_INFO` is the only entry point. On receipt it:

1. Queries `SystemInfoHardware::SystemInfoHal::getResetReason()` for the
   reset cause.
2. Reads the three commit constants from `SystemInfoVersion.hpp`.
3. Emits all four values as arguments of the `SystemInfo` event, writes each
   one to its own telemetry channel, and replies `OK`.

Hardware access is isolated behind `SystemInfoHardware::SystemInfoHal`
(`SystemInfoHardware.hpp`), which has two implementations selected by
`FPRIME_PLATFORM` in `CMakeLists.txt`:

- `SystemInfoHardware.cpp` — real target build. Decodes the PM `RCAUSE`
  register (datasheet §17.8.14). More than one `RCAUSE` flag can be set at
  once, so the decode is priority-ordered: `POWER_ON`, `BROWN_OUT_12`,
  `BROWN_OUT_33`, `EXTERNAL`, `WATCHDOG_TIMER`, `SYSTEM`, falling back to
  `UNKNOWN`.
- `SystemInfoHardwareStub.cpp` — Linux/UT build. Records call counts and lets
  tests inject the reason `getResetReason()` returns next
  (`setResetReason`), with a global `SystemInfoState` tests can inspect
  (`getSystemInfoState`) and reset between cases (`resetSystemInfoState`).
  It does not reimplement the `RCAUSE` decode — that logic is exercised only
  on hardware.

## 4. Ports

| Kind         | Name        | Port Type | Usage                                |
| ------------ | ----------- | --------- | ------------------------------------- |
| `time get`   | `timeCaller`| —         | Timestamp source for events/telemetry |

Plus the standard `Fw.Event`, `Fw.Command`, and `Fw.Channel` autocoded ports.

## 5. Commands

| Opcode | Name               | Arguments | Effect                                                        |
| ------ | ------------------ | --------- | -------------------------------------------------------------- |
| 0      | `EMIT_SYSTEM_INFO` | none      | Reads the current reset cause, emits the `SystemInfo` event, updates all four channels, replies `OK` |

## 6. Events

| Event        | Severity      | Arguments                                                     | Description                      |
| ------------ | ------------- | ------------------------------------------------------------- | --------------------------------- |
| `SystemInfo` | activity high | `resetReason`, `projectCommit`, `fprimeCommit`, `samdCommit`  | Reset cause + the three build commits, on `EMIT_SYSTEM_INFO` |

## 7. Telemetry

| Name            | Type                 | Update                | Description                         |
| --------------- | -------------------- | --------------------- | ------------------------------------ |
| `ResetCause`    | `Samd21.ResetReason` | On `EMIT_SYSTEM_INFO` | Cause of the most recent reset       |
| `ProjectCommit` | `U64` (hex)          | On `EMIT_SYSTEM_INFO` | Leading 64 bits of the project HEAD  |
| `FprimeCommit`  | `U64` (hex)          | On `EMIT_SYSTEM_INFO` | Leading 64 bits of the fprime HEAD   |
| `SamdCommit`    | `U64` (hex)          | On `EMIT_SYSTEM_INFO` | Leading 64 bits of the fprime-samd HEAD |

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

```fpp
instance systemInfo: Samd21.SystemInfo base id 0x3050
```

No port connections are required beyond `timeCaller` (wired to the
deployment's time component) and the autocoded command/event/telemetry ports
to the usual dispatcher/logger/channel components — `SystemInfo` has no
other inputs or outputs to wire.

## 10. Tested Configurations

| Build                      | Hardware source used          | Reset reasons exercised                                                   |
| --------------------------- | ------------------------------ | --------------------------------------------------------------------------- |
| `native-ut` (Linux)         | `SystemInfoHardwareStub.cpp`  | All of `ResetReason`: `POWER_ON`, `BROWN_OUT_12`, `BROWN_OUT_33`, `EXTERNAL`, `SYSTEM`, `WATCHDOG_TIMER`, `UNKNOWN` |
| `microchip_curiosity` (SAMD21G17A) | `SystemInfoHardware.cpp` | Not yet exercised against real `RCAUSE` resets; `SystemInfo` is instantiated (as `systemInfo`) in the `FFB` deployment topology but not `Breadboard` |

## 11. Limitations

- Instantiated in the `FFB` deployment topology (`FFB/FFB/instances.fpp`), not in `Breadboard`.
- Only reports a single reset cause even though multiple `RCAUSE` flags can
  be set simultaneously; the priority order in §3 determines which one is
  reported, and the others are silently dropped.
- Commit stamps are a single build-time snapshot; if the project, fprime, or
  fprime-samd tree is dirty (uncommitted changes) at build time, the reported
  commit does not reflect those changes.

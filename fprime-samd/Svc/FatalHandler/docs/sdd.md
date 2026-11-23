# Samd21::FatalHandler

Reports failed assertions to the ground as an `AssertionFailed` event, then resets the SAMD21.

## 1. Introduction

A failed `FW_ASSERT` on a baremetal SAMD21 leaves nothing to inspect: there is no console, and the
normal event path (logger, framer, DMA-driven UART) may be the thing that failed. `FatalHandler`
sends one fixed-size packet that names the failing file, line and arguments synchronously out of
`fatalOut`. It repeats the packet a configurable number of times so a ground listener can catch
one, then resets the MCU.

The component also ends a FATAL event: one routed to `FatalReceive` is turned into an assertion,
and follows the same path.

## 2. Requirements

| Name             | Description                                                                                                                      | Validation |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| SAMD21-FATAL-001 | FatalHandler shall receive failed assertions through the link-time selected `Fw_AssertHook` implementation without registering an `Fw::AssertHook`. | Unit Test  |
| SAMD21-FATAL-002 | FatalHandler shall send an `AssertionFailed` packet out of `fatalOut` `PACKET_REPEAT` times, `PACKET_DELAY_MS` apart, with interrupts disabled. | Unit Test  |
| SAMD21-FATAL-003 | The `AssertionFailed` packet shall carry the instance's `AssertionFailed` event ID.                                              | Unit Test  |
| SAMD21-FATAL-004 | Failed assertions shall be reported through the most recently constructed FatalHandler.                                           | Unit Test  |
| SAMD21-FATAL-005 | A failed assertion raised while no FatalHandler exists shall reset the MCU without reporting.                                     | Unit Test  |
| SAMD21-FATAL-006 | An assertion raised while an assertion is being reported shall reset the MCU without sending further packets.                     | Unit Test  |
| SAMD21-FATAL-007 | FatalHandler shall not send the packet when `fatalOut` is unconnected.                                                            | Unit Test  |
| SAMD21-FATAL-008 | FatalHandler shall reset the MCU after reporting an assertion.                                                                    | Unit Test  |
| SAMD21-FATAL-009 | FatalHandler shall assert when it receives a FATAL event.                                                                         | Unit Test  |
| SAMD21-FATAL-010 | Every copy of the `AssertionFailed` packet shall carry the time read once from `timeGet` when the assertion is reported.          | Unit Test  |

## 3. Design

### 3.1 Assert path

The framework turns a failed assertion into three calls on the link-time `Fw_AssertHook`
implementation. `Samd21_AssertHook_FatalHandler` (`FatalHandlerAssertHook.cpp`) forwards each one
to a static function on the component:

| Framework call               | FatalHandler        | Effect                                                                        |
| ---------------------------- | ------------------- | ----------------------------------------------------------------------------- |
| `Fw::formatAssertMessage()`  | `formatAssert()`    | Disable interrupts; read the time once; serialize the `AssertionFailed` event packet into the framework's `FW_ASSERT_TEXT_SIZE` buffer |
| `Fw::printAssertMessage()`   | `sendAssert()`      | Send that buffer out of `fatalOut` `PACKET_REPEAT` times, waiting `PACKET_DELAY_MS` after each |
| `Fw::terminateOnAssert()`    | `terminate()`       | `NVIC_SystemReset()`                                                          |

The instance is found through a file-scope `s_fatal_handler` pointer, which the constructor sets
to `this`. Nothing is registered at runtime, and no `Fw::AssertHook` code is linked.

If `s_fatal_handler` is unset, or `fatalOut` is unconnected, nothing is sent and the MCU still
resets. An assertion raised from inside the send (for example in the framer or UART driver) comes
back into `formatAssert()`. A guard sees that a report is already in progress and resets
immediately, rather than recursing.

Hardware access (interrupt disable, busy-wait delay, reset) is behind `FatalHandlerHardware`, with
a stub for native builds that records the calls.

### 3.2 Packet format

The packet has the `Fw::LogPacket` layout the downlink uses for every event, so the ground decodes
it as the dictionary's `AssertionFailed` event:

| Field       | Value                                                                  |
| ----------- | ---------------------------------------------------------------------- |
| descriptor  | `ComCfg.Apid.FW_PACKET_LOG`                                            |
| event ID    | `getIdBase() + EVENTID_ASSERTIONFAILED`                                |
| time tag    | `getTime()`, read once; every copy carries the same time               |
| arguments   | `file`, `lineNo`, `numArgs`, `arg1`..`arg6`, each argument as an `I32` |

The time is read through `timeGet` with interrupts already disabled, so the time source must be
synchronous and must not depend on interrupts. `Samd21.ProcTime` and `MoonfallPm.ScTime` (polled
RTC counter) both qualify. With `timeGet` unconnected the time is `TB_NONE` zero.

`file` is the assert file ID (`FW_FILEID_ASSERT`). Under the file-name assert levels used by unit
tests, it is reported as `0`.

The packet must fit both `Fw::ComBuffer` (`FW_COM_BUFFER_MAX_SIZE`) and the assert message buffer
(`FW_ASSERT_TEXT_SIZE`); both are checked with `static_assert`. A project can size
`FwAssertTextSize` in its `AcConstants.fpp` to the packet exactly (50 bytes with this project's

## 4. Ports

| Kind         | Name           | Port Type         | Usage                                                     |
| ------------ | -------------- | ----------------- | --------------------------------------------------------- |
| `sync input` | `FatalReceive` | `Svc.FatalEvent`  | A FATAL event was logged; asserts with the port and event ID |
| `output`     | `fatalOut`     | `Fw.Com`          | The `AssertionFailed` packet; connect to a synchronous framer |

Plus the standard `Fw.Event` port and the `timeGet` port, which supplies the packet's time tag. `AssertionFailed` is never sent through the
event port; it is only defined so the ground dictionary can decode the packet.

## 5. Configuration

The repeat count and spacing are set at compile time in `samd-config/FatalHandlerConfig.hpp`
(override via `CONFIGURATION_OVERRIDES`):

| Constant          | Default | Meaning                                                            |
| ----------------- | ------- | ------------------------------------------------------------------ |
| `PACKET_REPEAT`   | `10`    | Times the packet is sent before the MCU is reset                   |
| `PACKET_DELAY_MS` | `50`    | Busy-wait after each send; also lets the last byte leave the UART  |

The samd21 platform chooses `Samd21_AssertHook_FatalHandler` by default. A deployment without a
`FatalHandler` can choose `Samd21_AssertHook_reset` (in `Overrides/`) instead, which resets without
reporting:

```cmake
register_fprime_deployment(
    ...
    CHOOSES_IMPLEMENTATIONS
        Samd21_AssertHook_reset
)
```

There is no runtime `configure()`.

## 6. Integration

From the Breadboard deployment:

```fpp
instance fatalFramer: Samd21.SyncFramer base id FixedBaseIds.NO_DICTIONARY
instance fatalHandler: Samd21.FatalHandler base id 0x1010

connections FaultProtection {
    downlink.FatalAnnounce -> fatalHandler.FatalReceive
    fatalHandler.fatalOut  -> fatalFramer.comPacketIn
    fatalFramer.drvSendOut -> comDriver.sendSync
}
```

- **`fatalOut` must be connected:** without it, asserts reset silently.
- **`timeGet` should be connected** to the deployment's time component (the `time connections`
  pattern does this), and that component must read the time synchronously without interrupts.
- **The whole send path must be synchronous and polled.** Interrupts are disabled before the
  packet is sent, so `fatalOut` must reach the UART without any interrupt or DMA completion. Use
  `Samd21.SyncFramer` and the UART driver's `sendSync`, not the deployment's normal framer.
- **Keep it off the normal downlink.** Don't route `fatalOut` through the event logger or a
  queued framer; those may be what failed.
- **Use one instance.** Only the most recently constructed `FatalHandler` reports.

## 7. Tested Configurations

| Build                                      | Hardware source               | Notes                                    |
| ------------------------------------------ | ----------------------------- | ---------------------------------------- |
| `native-ut` (Linux)                        | `FatalHandlerHardwareStub.cpp` | Unit tests use `Samd21_AssertHook_FatalHandler`, the same as flight |
| `samd21g17` Breadboard (SAMD21G17A)        | `FatalHandlerHardware.cpp`     | Builds and links                         |
| `samd21j17` FFB (SAMD21J17D)               | `FatalHandlerHardware.cpp`     | Builds and links                         |

## 8. Limitations

- **Mid-flight DMA:** disabling interrupts does not stop an in-flight UART TX DMA transfer. If one
  is running when the assert fires, its bytes may interleave with the first copies of the packet.
  The later copies are sent once it drains.
- **Time source:** if the time component's read itself asserts, the recursion guard resets the
  MCU before anything is sent.
- **Assert window:** an assertion raised before the `FatalHandler` instance is constructed (static
  initialization of earlier topology objects) resets without reporting.
- **Blocking:** reporting blocks for about `PACKET_REPEAT * PACKET_DELAY_MS` (500 ms by default)
  plus the send time, with interrupts disabled. An external watchdog with a shorter timeout may
  reset the MCU first.
- **Argument width:** each assert argument is truncated to an `I32`.

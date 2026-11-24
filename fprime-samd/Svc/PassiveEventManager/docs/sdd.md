# Samd21::PassiveEventManager

Turns events into F Prime log packets and sends them to the downlink, without a queue.

## 1. Introduction

`PassiveEventManager` is a passive stand-in for `Svc.EventManager`. Each event received on `LogRecv`
is serialized into an `Fw.Com` log packet and sent out of `PktSend` on the caller's stack. FATAL
events are also announced on `FatalAnnounce`. With no thread, queue, or stored state, it fits a
baremetal deployment with no RAM to spare.

## 2. Requirements

| Name                           | Description                                                                                                                                                       | Validation |
| ------------------------------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| SAMD21-PASSIVEEVENTMANAGER-001 | PassiveEventManager shall send each event received on `LogRecv` out of `PktSend` as one F Prime log packet carrying the event ID, time tag and arguments.         | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-002 | PassiveEventManager shall send events of every severity, without filtering.                                                                                       | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-003 | PassiveEventManager shall send events whose arguments fit in a com buffer after the log packet header, from no arguments up to `FW_COM_BUFFER_MAX_SIZE` in total. | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-004 | PassiveEventManager shall announce each FATAL event's ID on `FatalAnnounce`, and shall not announce events of any other severity.                                 | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-005 | PassiveEventManager shall send a FATAL event's packet before announcing it, so the packet is out before the fatal handler takes over.                             | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-006 | PassiveEventManager shall drop, without asserting, an event whose log packet does not fit in a com buffer.                                                        | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-007 | PassiveEventManager shall announce a FATAL event whose log packet does not fit in a com buffer, even though the packet is dropped.                                | Unit Test  |
| SAMD21-PASSIVEEVENTMANAGER-008 | PassiveEventManager shall accept events, including FATAL events, with `PktSend` or `FatalAnnounce` unconnected.                                                   | Unit Test  |

## 3. Design

`LogRecv` builds the packet in a local `Fw::ComBuffer`, so the component keeps no state and a
re-entrant call (an event emitted from inside the `PktSend` call chain) gets its own buffer. The
packet layout matches `Svc.EventManager`: packet descriptor `FW_PACKET_LOG`, event ID, time tag,
then the argument bytes with no length prefix.

An event packet that does not fit in the com buffer is dropped. Event arguments are sized at
compile time, so this only happens for an event whose FPP arguments are too large for
`FW_COM_BUFFER_MAX_SIZE`. On FFB (`FW_COM_BUFFER_MAX_SIZE = 128`) the log packet header takes 16
bytes (1-byte descriptor, 4-byte ID, 11-byte time tag), which leaves 112 bytes for arguments. The
largest event in the FFB dictionary (`fatalHandler.AssertionFailed`) carries 34 bytes, so no FFB
event can be dropped. The check guards against a future event or configuration change, not
against runtime input.

## 4. Ports

| Kind         | Name            | Port Type         | Usage                                         |
| ------------ | --------------- | ----------------- | --------------------------------------------- |
| `sync input` | `LogRecv`       | `Fw.Log`          | Events from every component                   |
| `output`     | `PktSend`       | `Fw.Com`          | Log packet to the downlink (context is `0`)   |
| `output`     | `FatalAnnounce` | `Svc.FatalEvent`  | ID of a FATAL event, after its packet is sent |

## 5. Configuration

None. The component has no commands, events, telemetry, or parameters.

## 6. Integration

From the FFB deployment:

```fpp
instance events: Samd21.PassiveEventManager base id FixedBaseIds.NO_DICTIONARY

event connections instance events

connections Downlink {
    events.PktSend -> Gse.Subtopology.comDownlink
}

connections FaultProtection {
    events.FatalAnnounce   -> fatalHandler.FatalReceive
}
```

- `PktSend` runs in the context of whichever component emitted the event. The receiver must accept
  a call from any component's handler; `Samd21.Framer` buffers it until its next `schedIn` flush.
- `FatalAnnounce` is optional. When it is connected to `Samd21.FatalHandler.FatalReceive`, a FATAL
  event ends in an assertion.

## 7. Limitations

- No severity or ID filtering, and no commands to set any. Every event is sent.
- A FATAL event's packet is handed to `PktSend` before the announce. If the receiver buffers it (as
  `Samd21.Framer` does), the packet may never reach the ground once the fatal handler takes over.
  On FFB the event ID still reaches the ground: `Samd21.FatalHandler` turns the announce into an
  assertion whose `AssertionFailed` packet goes out synchronously, with the FATAL event ID as its
  second argument.
- A dropped oversized event is not reported. The component has no telemetry or event of its own to
  report it with, and an event about a dropped event would go back through the same port. See
  Design for why FFB cannot hit this.
- Only port index `0` of each output is used, so events go to one downlink.

## 8. Tested Configurations

| Board                    | Chip       | Downlink                                           |
| ------------------------ | ---------- | -------------------------------------------------- |
| Microchip Curiosity Nano | SAMD21G17A | Breadboard GDS over SERCOM0 USART                  |
| FFB                      | SAMD21J17D | GSE (SERCOM4) through `Samd21.Framer`              |

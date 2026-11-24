# Samd21::SyncFramer

Frames packets with the F Prime protocol and sends them to the driver synchronously.

## 1. Introduction

`SyncFramer` wraps each packet received on `comPacketIn` in an F Prime protocol frame (the same
frame `Svc.FprimeFramer` produces) and sends it out of `drvSendOut` before returning. It has no
buffers, queues, or completion callbacks, so it works with interrupts disabled. Its main user is
`Samd21.FatalHandler`.

## 2. Requirements

| Name                  | Description                                                                                                                                                                       | Validation |
| --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| SAMD21-SYNCFRAMER-001 | SyncFramer shall send each packet received on `comPacketIn` as one F Prime protocol frame: a header carrying the `0xDEADBEEF` start word and the packet length, the packet, and a trailer carrying the CRC32 of the header and packet. | Unit Test  |
| SAMD21-SYNCFRAMER-002 | Frames sent by SyncFramer shall be accepted by the F Prime frame detector used to deframe the stream on the ground.                                                               | Unit Test  |
| SAMD21-SYNCFRAMER-003 | SyncFramer shall frame packets of every size from 0 to `FW_COM_BUFFER_MAX_SIZE` bytes.                                                                                            | Unit Test  |
| SAMD21-SYNCFRAMER-004 | SyncFramer shall send the whole frame on `drvSendOut` before `comPacketIn` returns.                                                                                               | Unit Test  |
| SAMD21-SYNCFRAMER-005 | SyncFramer shall send the whole frame regardless of the status `drvSendOut` returns, without asserting.                                                                           | Unit Test  |
| SAMD21-SYNCFRAMER-006 | SyncFramer shall frame packets without waiting for `drvConnected`, and `drvConnected` shall have no effect.                                                                       | Unit Test  |

## 3. Ports

| Kind         | Name           | Port Type             | Usage                                       |
| ------------ | -------------- | --------------------- | ------------------------------------------- |
| `sync input` | `comPacketIn`  | `Fw.Com`              | Packet to frame and send                    |
| `sync input` | `drvConnected` | `Drv.ByteStreamReady` | Driver ready signal; ignored                |
| `output`     | `drvSendOut`   | `Drv.ByteStreamSend`  | Framed packet; connect to a synchronous send |

## 4. Configuration

None.

## 5. Integration

From the Breadboard deployment:

```fpp
instance fatalFramer: Samd21.SyncFramer base id FixedBaseIds.NO_DICTIONARY

connections FaultProtection {
    fatalHandler.fatalOut  -> fatalFramer.comPacketIn
    fatalFramer.drvSendOut -> comDriver.sendSync
}
```

`drvSendOut` must be connected to a send that is finished with the buffer when it returns, such as
`Samd21.UsartDriver.sendSync`. The component has no commands, events, telemetry, or parameters.

## 6. Limitations

- A frame may be sent in several pieces. Anything else written to the same driver while a frame
  is being sent ends up inside it, and the ground drops that frame.
- A failed send is not reported; the frame is lost.

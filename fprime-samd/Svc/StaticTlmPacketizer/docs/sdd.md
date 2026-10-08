# Samd21::StaticTlmPacketizer

A small bare-metal implementation of TlmPacketizer

## Introduction

`StaticTlmPacketizer` builds and downlinks packetized telemetry from a **compile-time**
packet table, as a drop-in replacement for `Svc::TlmPacketizer` on flash- and
RAM-constrained baremetal targets. It is passive, holds no member state, and has no
runtime packet table or `setPacketList()` call — the packet set is declared in the
deployment topology and turned into C++ by the `static-tlm-packetizer` build autocoder.
The wire format is unchanged from `Svc::TlmPacketizer`, so the same ground decoder
applies.

## Requirements

| Name | Description | Rationale | Validation |
|---|---|---|---|
| SAMD21-TLM-001 | Shall store the latest value of every channel assigned to a packet, in storage generated from the topology's packet set. | No runtime packet table on a 16 KB part. | Code inspection (generated `<Top>StaticTlmPacketAc.cpp`) |
| SAMD21-TLM-002 | Shall emit a packet on every connected `pktSendOut` port when requested by `pktSendIn` port index N or by `SEND_PKT(N)`. | Packets are either scheduler- or ground-driven; a packet may fan out to more than one downlink. | Code inspection |
| SAMD21-TLM-003 | Shall serialize each packet as packet descriptor, packet id, current time, then payload. | Wire compatibility with `Svc::TlmPacketizer`. | Code inspection |
| SAMD21-TLM-004 | Shall reject an unknown packet id with a `PacketNotFound` warning and an `EXECUTION_ERROR` response, emitting no packet. | `SEND_PKT` takes ground input. | Code inspection |
| SAMD21-TLM-005 | Shall require no runtime configuration, allocator, or task. | Baremetal deployments have no allocator. | Code inspection |

## Design

The autocoder reads the topology's packet set and generates, per deployment, one
statically sized byte array per packet plus the glue that copies each incoming channel
update into every packet that contains it. This is the component's only per-packet cost —
there is no runtime table, index map, or dynamic allocation. A channel that has never
been written downlinks as zeros, with no staleness indicator.

A packet send is triggered by:

- `pktSendIn`, where the port index is the packet id
- the `SEND_PKT(id)` ground command

Sending serializes the following, in order, into an `Fw::ComBuffer`, then emits it on
every connected index of `pktSendOut` (looping `0..NUM_PKT_OUT`, skipping unconnected
indices) — this is how one packet reaches two independent downlinks, e.g. a GSE path and
a flight-computer path:

- the packet descriptor
- the packet id
- the current time
- the payload

Every send emits the whole packet — there is no on-change, rate, or section logic.

Failure modes:

- **Unknown packet id** — logged as `PacketNotFound(id)`; `SEND_PKT` replies
  `EXECUTION_ERROR` and no packet is emitted.
- **Packet larger than the `Fw::ComBuffer`** — runtime FATAL assert; there is no
  build-time size check.

## Usage / Topology Integration


**1. Enable the autocoder**, once per project, in the root `CMakeLists.txt` between the
`FPrime.cmake` include and `fprime_setup_included_code()`. It resolves out of
`lib/fprime-samd/cmake/autocoder/`, so `lib/fprime-samd` must be in `settings.ini`
`library_locations`:

```cmake
register_fprime_build_autocoder("autocoder/static_tlm_packet" OFF)
```

This is project-global: it applies to every module with a topology, so **every**
deployment in the project needs a `telemetry packets` set (step 3), whether or not it
instantiates the packetizer.

**2. Force the topology archive to link whole.** The generated code lands in
`lib<Deployment>_Top.a`, which the linker sees before the component archive that needs
it, so the link fails on undefined symbols unless the archive is force-linked. In the
**deployment** `CMakeLists.txt` (see `Breadboard/CMakeLists.txt`), before
`register_fprime_deployment()`:

```cmake
set(CMAKE_CXX_LINK_LIBRARY_USING_WHOLE_ARCHIVE
    "-Wl,--whole-archive" "<LINK_ITEM>" "-Wl,--no-whole-archive")
set(CMAKE_CXX_LINK_LIBRARY_USING_WHOLE_ARCHIVE_SUPPORTED TRUE)
```

and after it, once the target exists:

```cmake
set_property(TARGET "${FPRIME_CURRENT_MODULE}"
    PROPERTY "LINK_LIBRARY_OVERRIDE_${FPRIME_CURRENT_MODULE}_Top" WHOLE_ARCHIVE)
```

`Top/CMakeLists.txt` needs nothing beyond the stock topology registration.

**3. Declare the packet set** in the deployment topology. Every telemetry channel in the
topology must appear either in a packet or in the trailing `omit` block. From
`Breadboard/Breadboard/topology.fpp`:

```fpp
        telemetry packets Main {

            packet Error group 1 {
                i2cDriver.BusErrorCount
                framer.DroppedPackets
                testSensor.I2cErrors
            }

            ...

        } omit {
            rg8Hz.CycleTime
            ...
        }
```

- Packet ids default to declaration order, so reordering packets silently renumbers
  them. Pin them with `packet <Name> id <N> group <G>` if the ground side depends on
  them.
- Only fixed-width serializables may appear in a packet — a string-typed channel fails
  the build; string channels in `omit` are fine.

**4. Declare the instance.** No configuration or init phase code, one line. At most one
instance per topology; a second is a hard autocoder error:

```fpp
    instance tlm: Samd21.StaticTlmPacketizer base id 0x2030
```

**5. Wire it.** `tlmRecvIn`, `timeCaller`, and the command/event AC ports all come from
the FPP pattern specifiers — `telemetry connections instance tlm` is mandatory, since
`Samd21.PassiveDownlink` has no `Fw.Tlm` input port:

```fpp
        command connections instance cmdDisp

        event connections instance downlink

        telemetry connections instance tlm

        time connections instance timeHandler
```

`pktSendOut` is an array port, sized by `Samd21.StaticTlmPacketizerConfig.NUM_PKT_OUT`,
and must be hand-wired. Every connected index gets a copy of every packet sent — this is
how the same packet set reaches more than one downlink. `Breadboard` wires a single
index into the event downlink's framer:

```fpp
        connections Link {
            # Telemetry downlink pipeline
            downlink.PktSend        -> framer.comPacketQueueIn
            tlm.pktSendOut          -> framer.comPacketQueueIn
```


```fpp
        tlm.pktSendOut[Ports_PacketTransmit.F1] -> framer1.comPacketQueueIn
        tlm.pktSendOut[Ports_PacketTransmit.F2]  -> framer2.comPacketQueueIn
```

Neither deployment connects `pktSendIn`, so packets today are emitted only by the
`SEND_PKT` ground command. To schedule one, connect a rate group member to the port
index equal to the packet id, e.g. `rg1Hz.RateGroupMemberOut[N] -> tlm.pktSendIn[0]` for
packet id 0 — and raise `Samd21.StaticTlmPacketizerConfig.NUM_TLM_PACKETS` past the
highest id used this way (see Configuration).

## Configuration

All configuration is compile-time; there is no `configure()`, `setup()`, allocator, or
`configComponents` phase code.

Both constants live in a shared `Samd21.StaticTlmPacketizerConfig` module (not bare
`Samd21` constants) so a project overriding one does not have to also redeclare the
other.

| Setting | Where | Default | Effect |
|---|---|---|---|
| `Samd21.StaticTlmPacketizerConfig.NUM_TLM_PACKETS` | `lib/fprime-samd/default/samd-config/Samd21StaticTlmPacketizerConfig.fpp` | 1 | Size of `pktSendIn`. Must be **strictly greater than the largest packet id** driven from a port. |
| `Samd21.StaticTlmPacketizerConfig.NUM_PKT_OUT` | same file | 1 | Size of `pktSendOut` |

| Setting | Where | Value | Effect |
|---|---|---|---|
| `FW_COM_BUFFER_MAX_SIZE` | `config-moonfallpm/FpConstants.fpp` | 128 | `Fw::ComBuffer` size. Per-packet payload budget is this minus the 14-byte header. |
| `FwTlmPacketizeIdType` | `config-moonfallpm/FpConfig.fpp` | `U16` | Packet id width, on the wire and in `SEND_PKT`. |
| `FwPacketDescriptorType` | `config-moonfallpm/ComCfg.fpp` | `U8` | Packet descriptor width in the header. |

The header is 14 bytes (descriptor 1 + id 2 + `Fw::Time` 11), leaving a 114-byte payload
budget. Static RAM cost is the sum of all packet payloads, paid unconditionally.

## Limitations

- `pktSendIn` is sized by `NUM_TLM_PACKETS`, defaulted to 1 in the library — a project
  with more scheduler-driven packets must override it (see Configuration). Neither
  deployment overrides it today, so `pktSendIn` is effectively unusable as shipped.
- `pktSendOut` fans the same packet out to every connected index unconditionally —
  there is no per-index filtering, so a packet cannot be routed to only one of several
  downlinks without an external filter on the receiving side.
- No build-time oversize check: a packet larger than the payload budget asserts at
  runtime on first send.
- An unpacketized channel is a silent no-op; a channel id outside the dictionary is a
  FATAL assert. The declared `NoChan` event is never emitted.
- No change tracking and no critical section around the packet buffers.
- No bounds check against the incoming `Fw::TlmBuffer` length when copying a channel.
- No unit test: `register_fprime_ut()` is commented out in `CMakeLists.txt` and `test/`
  is empty.

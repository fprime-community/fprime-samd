# Samd21::StaticCmdDispatcher

A small bare-metal implementation of CmdDispatcher

## Introduction

`StaticCmdDispatcher` is
[`Baremetal::PassiveCmdDispatcher`](../../../../../fprime-baremetal/fprime-baremetal/Svc/PassiveCmdDispatcher/docs/sdd.md)
with the runtime opcode-registration table replaced by a compile-time opcode-to-port
lookup autocoded from the deployment topology. The port, command, and event set is
otherwise the same, so **anything not listed under Delta behaves as documented there**.
Requirements are likewise inherited.

## Delta

| Area            | `Baremetal::PassiveCmdDispatcher`                                                   | `Samd21::StaticCmdDispatcher`                                                                                                                                                                                                          |
| --------------- | ----------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Opcode → port   | Runtime `m_entryTable`, linear scan per dispatch                                    | `lookupDispatchPort()` — a `switch` on the opcode with `base id + opcode` case labels, autocoded at the topology level                                                                                                                 |
| Bring-up        | `setup(memId, allocator)` required, allocator-backed tables                         | None. The constructor initializes everything; no `Fw::MemAllocator`                                                                                                                                                                    |
| RAM             | `DispatchEntry[CMD_DISPATCHER_DISPATCH_TABLE_SIZE]` (128 entries) allocated at boot | Gone. Only the sequence tracker remains, as a plain member array                                                                                                                                                                       |
| `compCmdReg`    | Stores `{opcode, port}`                                                             | Stores nothing. Asserts the autocoded table resolves the opcode to the port the registration arrived on — a wiring/table disagreement is a FATAL at boot                                                                               |
| `regCommands()` | Required                                                                            | Optional; when called it is purely that boot-time consistency check                                                                                                                                                                    |
| Events          | —                                                                                   | Same names, ids, severities and formats. The opcode argument is spelled `opCode` rather than `$opcode`, and `OpCodeDispatched.$port` is `I32` rather than `FwIndexType` (the same type on SAMD21). Dictionary-visible, wire-identical. |

## Usage / Topology Integration

Snippets are verbatim from `Breadboard_Curiosity/Top/`; `FFB_Tester/Top/` is identical
apart from indentation. Paths are relative to the project root.

**1. Enable the autocoder**, once per project, in the root `CMakeLists.txt` between the
`FPrime.cmake` include and `fprime_setup_included_code()`. It resolves out of
`lib/fprime-samd/cmake/autocoder/`, so `lib/fprime-samd` must be in `settings.ini`
`library_locations`:

```cmake
register_fprime_build_autocoder("autocoder/static_cmd_dispatch" OFF)
```

**2. Force the topology archive to link whole**, in the deployment `CMakeLists.txt`.
Identical to the requirement documented for
[`StaticTlmPacketizer`](../../StaticTlmPacketizer/docs/sdd.md) — `lookupDispatchPort` is
defined in `lib<Deployment>_Top.a`, so without it the link fails on that undefined symbol.

**3. Declare the instance.** No configuration or init phase code:

```fpp
    instance cmdDisp: Samd21.StaticCmdDispatcher base id 0x2020
```

**4. Wire it.**

```fpp
        command connections instance cmdDisp
```

```fpp
        connections Uplink {
            # Router <-> CmdDispatcher
            fprimeRouter.commandOut -> cmdDisp.seqCmdBuff
            cmdDisp.seqCmdStatus    -> fprimeRouter.cmdResponseIn
        }
```

Build-time constraints:

- At most one `Samd21.StaticCmdDispatcher` instance per topology.
- Instance base ids must not collide: opcodes are emitted as `case 0x<base id> + 0x<opcode>:`,
  so an overlap is a duplicate-case compile error rather than a runtime failure.
  `0xFFFFFFFF` is reserved (`OPCODE_UNUSED`).
- The `@ static-cmd-dispatcher` annotation on the component is the autocoder's discovery
  key, and the dispatch port name `compCmdSend` is hardcoded in
  `lib/fprime-samd/tools/static-cmd-dispatcher`. Change either and the generated file
  comes out without a `lookupDispatchPort` definition, failing the link.
- Generation runs a Python script importing `fpp` and `fprime_cpp_codegen`, so CMake
  configure must run inside the project venv.

At runtime, an opcode absent from the table falls through to the `switch` default and is
reported as `InvalidCommand` plus `INVALID_OPCODE` on `seqCmdStatus`. The generated table
is at `<build-dir>/<Deployment>/Top/<TopologyName>StaticCmdDispatchAc.cpp`, with each case
block commented with the target instance's qualified name, base id, and opcodes.

## Configuration

No runtime configuration. Sizing comes from the deployment's config module, which must
also supply `CommandDispatcherImplCfg.hpp` (the component includes it with no `DEPENDS`
of its own). Values shown are this project's:

| Setting                               | Where                                            | Value | Bounds                                                                                                  |
| ------------------------------------- | ------------------------------------------------ | ----- | ------------------------------------------------------------------------------------------------------- |
| `CmdDispatcherComponentCommandPorts`  | `config-moonfallpm/AcConstants.fpp`              | 30    | Commanded instances on `compCmdSend`/`compCmdReg`                                                       |
| `CmdDispatcherSequencePorts`          | `config-moonfallpm/AcConstants.fpp`              | 5     | Command sources on `seqCmdBuff`/`seqCmdIn`/`seqCmdStatus`; also bounds `SET_EVENT_EMISSION`'s `portIdx` |
| `CMD_DISPATCHER_SEQUENCER_TABLE_SIZE` | `config-moonfallpm/CommandDispatcherImplCfg.hpp` | 4     | Simultaneously outstanding commands; overflow yields `TooManyCommands` + `EXECUTION_ERROR`              |

`CMD_DISPATCHER_DISPATCH_TABLE_SIZE` in the same header is unused by this component.

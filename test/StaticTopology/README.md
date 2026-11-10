# StaticTopology

A native-only deployment topology that the unit tests of fprime-samd's static components
(`Svc/StaticCmdDispatcher`, `Svc/StaticTlmPacketizer`) run against.

Those components get their dispatch tables and packet layouts from the `static-*` build
autocoders, which only run on normal modules (never on a unit-test target) and need a
deployment topology as input. So the topology is built once here, as a module, with the real
generated code, and each component's unit test links it and drives the real instances.
Coverage lands on the component's own module because its test is registered there.

## Layout

| Directory | Contents |
|---|---|
| `Top/` | `instances.fpp`, `topology.fpp` and the shared gtest fixture (`StaticTopologyFixture`) |
| `Target/` | Command and telemetry endpoint: a command that records a value, one that completes with a chosen response, one held until the test releases it, and a channel of every type |
| `Ground/` | Ground/sequencer stand-in: sends command packets or `Fw.Cmd` commands, records responses, triggers and records packets. One instance per dispatcher sequence port |
| `EventLog/` | Records every event |
| `Clock/` | Time source the test sets |

A new edge case of a generator is covered by adding to the topology (an instance, a channel, a
packet, a harness component), not by hand-writing the generated code.

## Building

Opt-in, since the topology compiles against the project's configuration:

```bash
fprime-util generate --ut -DFPRIME_SAMD_STATIC_TOPOLOGY_TESTS=ON
fprime-util check -p lib/fprime-samd/fprime-samd/Svc/StaticCmdDispatcher
```

A component registers its test with `fprime_samd_add_static_topology_ut` (see
`cmake/static_topology.cmake`), which links the topology whole-archive: the component calls the
generated functions and the topology constructs the component, so the two libraries depend on
each other.

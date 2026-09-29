# Samd21::Seq

A component for dispatching a static command list on a schedule.

## 1. Introduction

`Seq` runs pre-built command sequences ("action tables") stored in
flash. Sequences are compiled at build time from `.seq` files into byte blobs
— there's no filesystem or spare RAM to hold an uplinked sequence, so
everything is static and selected by name (`Samd21.SeqNames`).

The component walks the blob, dispatches one command at a time to a command
dispatcher, and waits for its response before dispatching the next. Blocking
delays are just commands (`WAIT_TICKS`, `WAIT_UNTIL`) inside the sequence
itself.

## 2. Requirements

| Name         | Description                                                                 | Validation |
| ------------ | --------------------------------------------------------------------------- | ---------- |
| MFPM-ACT-001 | Dispatch the commands of a named sequence in order, one at a time.          | Unit Test  |
| MFPM-ACT-002 | Advance only after the in-flight command's response is `OK`.                | Unit Test  |
| MFPM-ACT-003 | Abort the rest of the sequence when a command fails.                        | Unit Test  |
| MFPM-ACT-004 | `RUN` pends a sequence; it starts once the component is idle.               | Unit Test  |
| MFPM-ACT-005 | `RUN_ON_ERROR` arms a one-shot recovery sequence run on failure.            | Unit Test  |
| MFPM-ACT-006 | `WAIT_TICKS` / `WAIT_UNTIL` block the sequence until their condition holds. | Unit Test  |
| MFPM-ACT-007 | `CANCEL` stops execution and resets the program counter to the start.       | Unit Test  |
| MFPM-ACT-008 | A malformed record is rejected and ends the sequence.                       | Unit Test  |

## 3. Design

`Seq` is a passive component driven by two ticks and a state
machine with one command in flight at a time:

- **`activeIn`** starts a pending sequence and dispatches the next command
  once the previous one's response is `OK`.
- **`schedIn`** services an active `WAIT_TICKS` / `WAIT_UNTIL` sleep and
  resumes dispatch once it expires.
- A failing command response aborts the rest of the sequence; a
  `RUN_ON_ERROR`-armed sequence (if any) is pended in its place.

`WAIT_TICKS` / `WAIT_UNTIL` are ordinary commands dispatched like any other
record — their handlers just record the wait and reply `OK`, which drives the
component into a sleeping state instead of advancing immediately.

## 4. Sequence Format

A sequence is a flat list of records in a flash-resident byte blob, with no
header/footer:

```
U32  cmdSize     length of the command that follows
U8[] cmd         serialized opcode + arguments
```

repeated to the end of the blob. There are no time tags — timing is expressed
with explicit `WAIT_TICKS` / `WAIT_UNTIL` commands in the sequence.

### 4.1 Writing a `.seq`

Sequences are written using the normal `fprime-seqgen` `.seq` syntax and
built against the deployment's dictionary, so a typo or bad command fails
the build. Time tags in the file are ignored (write `R00:00:00` always);
use `WAIT_TICKS` / `WAIT_UNTIL` for delays:

```
; SENSORS.seq
R00:00:00 FFB_Tester.seq.RUN_ON_ERROR SENSORS_RECOVER

R00:00:00 FFB_Tester.pwr1v8.READ_SENSE
R00:00:00 FFB_Tester.pwr1v8.READ_VOLTAGE
R00:00:00 FFB_Tester.pwr1v8.READ_POWER

; Loop: re-run this sequence, then wait 7 ticks (8Hz -> 1s cadence)
R00:00:00 FFB_Tester.seq.RUN SENSORS
R00:00:00 FFB_Tester.seq.WAIT_TICKS 7
```

### 4.2 Build Pipeline

`cmake/seqs.cmake`'s `fprime_add_static_sequence()` turns each
`seqs/<NAME>.seq` into a linkable `fprime_seq_<NAME>[]` / `_len` symbol pair:

1. `fprime-seqgen` assembles the `.seq` against the topology dictionary into
   a `.bin`.
2. `cmake/seq_to_c.py` strips the seqgen header/footer and each record's
   descriptor + time tag, leaving the `(cmdSize, cmd)` list above, and emits
   a `.c`/`.h` pair.

## 5. Commands

| Command        | Arguments                         | Effect                                                                 |
| -------------- | --------------------------------- | ---------------------------------------------------------------------- |
| `RUN`          | `table: SeqNames`                 | Pend `table`; starts once idle.                                        |
| `RUN_ON_ERROR` | `table: SeqNames`                 | Arm `table` to run if the current command fails. Clears pending `RUN`. |
| `CANCEL`       | —                                 | Stop and reset the program counter. Clears pending/armed runs.         |
| `WAIT_TICKS`   | `n: U32`                          | Block for `n` `schedIn` ticks (`n=0` = resume next tick).              |
| `WAIT_UNTIL`   | `timeBase`, `seconds`, `useconds` | Block until the FSW clock reaches this time on this base.              |

## 6. Ports

| Kind         | Name                | Port Type         | Usage                                          |
| ------------ | ------------------- | ----------------- | ---------------------------------------------- |
| `sync input` | `activeIn`          | `Svc.ActiveSched` | Main-context tick: start/dispatch next command |
| `sync input` | `schedIn`           | `Svc.Sched`       | Rate-group tick: service `WAIT_*` sleeps       |
| `output`     | `commandOut`        | `Fw.Com`          | Dispatch a command to a dispatcher             |
| `sync input` | `commandResponseIn` | `Fw.CmdResponse`  | Response to the in-flight command              |

## 7. Usage

### 7.1 Configuring Sequence Names

Override the `Samd21.SeqNames` dictionary enum per deployment:

```fpp
module Samd21 {
    dictionary enum SeqNames : U8 {
        STARTUP
        SENSORS
        SENSORS_RECOVER
    } default STARTUP
}
```

### 7.2 Wiring Up the Component

```fpp
instance seq: Samd21.Seq base id 0xAA40

connections {
  cycler.cycleOut         -> seq.activeIn
  rg8Hz.RateGroupMemberOut -> seq.schedIn

  seq.commandOut          -> cmdDisp.seqCmdBuff[1]
  cmdDisp.seqCmdStatus[1] -> seq.commandResponseIn
}
```

- `activeIn` must be connected or nothing ever dispatches.
- `schedIn` must be connected or `WAIT_*` never wakes.
- `commandOut`/`commandResponseIn` must use the same dispatcher port index.

### 7.3 Loading and Starting Sequences

```cpp
// Indexed by SeqNames
static const Samd21::Seq::Action seqs[Samd21::SeqNames::NUM_CONSTANTS] = {
    Samd21::Seq::Action(fprime_seq_STARTUP, fprime_seq_STARTUP_len),
    Samd21::Seq::Action(fprime_seq_SENSORS, fprime_seq_SENSORS_len),
    Samd21::Seq::Action(fprime_seq_SENSORS_RECOVER, fprime_seq_SENSORS_RECOVER_len),
};

seq.configure(seqs);
seq.run(Samd21::SeqNames::STARTUP);  // starts on first activeIn tick
```

`run()` pends a sequence from topology setup (e.g. to auto-start at boot); the
`RUN` command does the same thing from ground.

## 8. Limitations

- Sequence length limited to 64KB (`U16` program counter).
- No telemetry. Execution is observable only via events.

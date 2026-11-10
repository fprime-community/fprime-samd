# Samd21::FramPeekPoke

Ground access to raw F-RAM words.

## Introduction

`FramPeekPoke` reads (`PEEK`) or writes (`POKE`) a single U8, U16 or U32 word at any F-RAM byte offset
through one `Fram.FramDriver` client index. It is a flight debugging and recovery tool. Use it to patch a
stored value by hand (for example a parameter record in a `Samd21.PrmStorage` bank) or to inspect stored
data (for example a suspected bit flip).

A `POKE` bypasses every integrity check on the data it lands in. Poking a `StaticPrmDb` record without
also fixing that record's CRC-16 leaves the record (and only its parameter) invalid.

## Requirements

| Name | Description | Validation |
|---|---|---|
| FRAM-PP-001 | `PEEK(offset, width)` shall read `width` bytes at `offset`, report the big-endian value in `Peeked`, and complete `OK` when the read does. | Unit Test |
| FRAM-PP-002 | `POKE(offset, width, value)` shall write exactly `width` bytes of `value`, big endian, at `offset`, report `Poked`, and complete `OK` when the write does. | Unit Test |
| FRAM-PP-003 | A `POKE` value that does not fit `width` shall fail with `VALIDATION_ERROR` and `ValueTooWide`, and touch no F-RAM. | Unit Test |
| FRAM-PP-004 | A request the F-RAM driver rejects, or completes with an error, shall fail the command with `EXECUTION_ERROR` and `AccessFailed`. | Unit Test |
| FRAM-PP-005 | A command received while one is outstanding shall complete `BUSY`. | Unit Test |
| FRAM-PP-006 | The component shall work when the F-RAM completion arrives inside the request call. | Unit Test |

## Design

Each command is one F-RAM request out of a 4-byte component buffer. The command response is sent from
the F-RAM completion, or immediately if the request is rejected. Range checks (offset and width against
`Fram.FramConfig.Size`) and the block-protect check are left to the F-RAM driver. Its rejection status
appears in `AccessFailed`. A request rejected `BUSY` (for example during the driver's startup reads, or
while a `PrmStorage` transfer holds the device) is not retried; the operator re-sends the command.

### Ports

| Port | Kind | Connects to |
|---|---|---|
| `framRead` / `framReadComplete` | out / sync in | `framDriver.read[i]` / `framDriver.readComplete[i]` |
| `framWrite` / `framWriteComplete` | out / sync in | `framDriver.write[i]` / `framDriver.writeComplete[i]` |

### Commands and events

| Item | Kind | Meaning |
|---|---|---|
| `PEEK(offset, width)` | command | Read a word |
| `POKE(offset, width, value)` | command | Write a word |
| `Peeked` | activity high | Value read |
| `Poked` | activity high | Value written |
| `ValueTooWide` | warning low | `POKE` value does not fit the width |
| `AccessFailed` | warning high | The F-RAM driver rejected or failed the request |

## Configuration

None. The component is only built when the project's library locations include fprime-devices.

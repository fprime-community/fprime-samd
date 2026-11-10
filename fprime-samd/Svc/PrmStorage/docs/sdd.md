# Samd21::PrmStorage

F-RAM backing store for the `Samd21.StaticPrmDb` records, with selectable banks.

## Introduction

`Samd21.StaticPrmDb` moves its store as a series of `Samd21.PrmTransfer(offset, buffer)` calls
(`writeOut` / `readOut`). It expects one `Drv.ByteStreamData` completion back for each. `Fram.FramDriver`
takes requests that are addressed by offset and limited to `Fram.FramConfig.MaxTransfer` bytes.
`PrmStorage` connects the two. It splits each transfer into chunks, sends them at the transfer's offset
within the selected bank, and returns the buffer to the database once the last chunk completes.

The bytes pass through unchanged. Validating the records (per-record CRC and ID) is the database's
job. Overlaying the banks is done by the deployment's sequences, using `SELECT_BANK` and
`LOAD MERGE`.

## Requirements

| Name | Description | Validation |
|---|---|---|
| PRM-STORE-001 | `writeIn` / `readIn` shall transfer the buffer, unchanged, to / from its offset within the selected bank in `MaxTransfer` chunks, and return `OP_OK` on `writeReturnOut` / `readReturnOut` after the last chunk. | Unit Test |
| PRM-STORE-002 | A failed chunk shall end the transfer with `OTHER_ERROR` and emit `StorageFailed`. | Unit Test |
| PRM-STORE-003 | `SELECT_BANK` shall point the following transfers at the given bank. During a transfer it shall be rejected `BUSY` and the transfer shall keep its bank. | Unit Test |
| PRM-STORE-004 | A request the F-RAM driver rejects `BUSY` shall be retried on `schedIn`, up to `BUSY_RETRY_LIMIT` times. Any other rejection, or running out of retries, shall fail the transfer. | Unit Test |
| PRM-STORE-005 | The component shall work when the F-RAM completion arrives inside the request call. | Unit Test |
| PRM-STORE-006 | The build shall fail when the parameter store does not fit both banks inside the F-RAM or the banks overlap. | Inspection (static_assert) |

## Design

Byte `i` of a transfer at `offset` is stored at `bankOffset[selected] + offset + i`. One request is outstanding at a time. The
completion of one chunk issues the next. `PRIME` is selected at boot.

### Ports

| Port | Kind | Connects to |
|---|---|---|
| `writeIn` / `writeReturnOut` | sync in / out | `prm.writeOut` / `prm.writeReturnIn` |
| `readIn` / `readReturnOut` | sync in / out | `prm.readOut` / `prm.readReturnIn` |
| `framRead` / `framReadComplete` | out / sync in | `framDriver.read[i]` / `framDriver.readComplete[i]` |
| `framWrite` / `framWriteComplete` | out / sync in | `framDriver.write[i]` / `framDriver.writeComplete[i]` |
| `schedIn` | sync in | A rate group; retries a request the driver rejected `BUSY` |

### Commands, telemetry, events

| Item | Kind | Meaning |
|---|---|---|
| `SELECT_BANK(bank)` | command | Bank used by the following saves and loads; `BUSY` during a transfer |
| `ActiveBank` | telemetry | Bank of the next transfer |
| `StorageFailed` | warning high | An F-RAM request failed; the transfer returned `OTHER_ERROR` (throttled, 5) |

## Configuration

Compile-time, in `samd-config/PrmStorageConfig.hpp` (override via `CONFIGURATION_OVERRIDES`):

| Constant | Meaning |
|---|---|
| `PRIME_OFFSET` | F-RAM offset of the PRIME bank |
| `BACKUP_OFFSET` | F-RAM offset of the BACKUP bank |
| `BUSY_RETRY_LIMIT` | `schedIn` ticks a `BUSY`-rejected request is retried |

A bank holds one parameter store of `StaticPrmDbConfig::STORE_RECORDS * PrmRecord::SIZE` bytes.
`PrmStorage` static_asserts `PrmStorageLayout::banksHold` with that size, so the build fails if either
bank runs past `Fram.FramConfig.Size` or the banks overlap.

The component is only built when the project's library locations include fprime-devices (it uses the
`Fram` ports).

## Usage in FFB

`STARTUP` selects `PRIME` and runs `prm.LOAD REPLACE`. `PRM_LOAD_BACKUP` then always selects `BACKUP`
and runs `prm.LOAD MERGE`, which fills in any parameter PRIME did not hold a valid record for. An F-RAM
error in either load moves the sequence on through `RUN_ON_ERROR`. Both paths end in `PRM_LOAD_DONE`,
which selects `PRIME` again and starts `SENSORS`. To keep both banks current, a save is `SELECT_BANK PRIME`,
`prm.SAVE`, `SELECT_BANK BACKUP`, `prm.SAVE`.

# Samd21::PrmStorage

F-RAM backing store for the `Samd21.StaticPrmDb` image, with selectable banks.

## Introduction

`Samd21.StaticPrmDb` sends its whole database image out as a single `Fw.Buffer` (`writeOut` / `readOut`)
and expects one `Drv.ByteStreamData` completion back. `Fram.FramDriver` takes requests that are addressed
by offset and limited to `Fram.FramConfig.MaxTransfer` bytes. `PrmStorage` connects the two. It splits
the image into chunks, sends them at the offset of the selected bank, and returns the image to the
database once the last chunk completes.

The image passes through unchanged. Validating it (dictionary hash, per-record CRC) is the database's
job. Falling back between banks is done by the deployment's sequences, using `SELECT_BANK`.

## Requirements

| Name | Description | Validation |
|---|---|---|
| PRM-STORE-001 | `writeIn` / `readIn` shall transfer the image, unchanged, to / from the selected bank in `MaxTransfer` chunks, and return `OP_OK` on `writeReturnOut` / `readReturnOut` after the last chunk. | Unit Test |
| PRM-STORE-002 | A failed chunk shall end the transfer with `OTHER_ERROR` and emit `StorageFailed`. | Unit Test |
| PRM-STORE-003 | `SELECT_BANK` shall point the following transfers at the given bank. During a transfer it shall be rejected `BUSY` and the transfer shall keep its bank. | Unit Test |
| PRM-STORE-004 | A request the F-RAM driver rejects `BUSY` shall be retried on `schedIn`, up to `BUSY_RETRY_LIMIT` times. Any other rejection, or running out of retries, shall fail the transfer. | Unit Test |
| PRM-STORE-005 | The component shall work when the F-RAM completion arrives inside the request call. | Unit Test |
| PRM-STORE-006 | The build shall fail when the database image does not fit both banks inside the F-RAM or the banks overlap. | Inspection (static_assert) |

## Design

Byte `i` of the image is stored at `bankOffset[selected] + i`. One request is outstanding at a time. The
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

A bank is exactly one database image. Only the autocoded `StaticPrmDb` knows the image size, so the layout
is checked there: the generated file asserts `StaticPrmDbConfig::storageFits(DATABASE_SIZE)`. A deployment
storing the image with `PrmStorage` overrides `samd-config/StaticPrmDbConfig.hpp` to return
`PrmStorageLayout::banksHold(imageSize)`. That fails the build if either bank runs past
`Fram.FramConfig.Size` or the banks overlap.

The component is only built when the project's library locations include fprime-devices (it uses the
`Fram` ports).

## Usage in FFB

`STARTUP` arms `RUN_ON_ERROR PRM_LOAD_BACKUP`, selects `PRIME` and runs `prm.LOAD`. If the load fails
(blank bank, dictionary-hash mismatch or F-RAM error), the sequencer aborts `STARTUP` and runs
`PRM_LOAD_BACKUP`, which tries the same load on `BACKUP`. Both paths end in `PRM_LOAD_DONE`, which
selects `PRIME` again and starts `SENSORS`. To keep both banks current, a save is `SELECT_BANK PRIME`,
`prm.SAVE`, `SELECT_BANK BACKUP`, `prm.SAVE`.

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

| Name | Description | Rationale | Validation |
|---|---|---|---|
| PRM-STORE-001 | A save shall store the database's bytes unchanged at their offset within the selected bank, and a load shall return the bytes stored there unchanged. A transfer shall complete successfully only once every one of its bytes has been transferred. | Record format and integrity checks belong to `StaticPrmDb`; storage that reordered, interpreted or partially transferred the bytes would defeat the per-record CRCs. | Unit Test |
| PRM-STORE-002 | Each F-RAM request shall be at most `Fram.FramConfig.MaxTransfer` bytes, with one request outstanding at a time. | The F-RAM driver's frame buffers are sized for `MaxTransfer`, and the driver is shared with other clients. | Unit Test |
| PRM-STORE-003 | A failed F-RAM request shall end the transfer with an error, issue no further requests for it, and report the direction, bank, F-RAM offset and status of the failure. | The database fails the command and the redundant bank can fill in; the operator needs to locate the failing region of the device. | Unit Test |
| PRM-STORE-004 | The storage shall hold the store in two banks, PRIME and BACKUP, and direct every following transfer to the bank last selected by command. PRIME shall be selected at boot, and the selected bank shall be reported in telemetry. | Redundant copies let a parameter survive corruption of one bank; which bank is loaded first is the deployment sequence's decision. | Unit Test |
| PRM-STORE-005 | A bank selection received during a transfer shall be rejected busy, and the transfer shall complete on the bank it started on. | A transfer split across banks would mix two stores, and the merge order across banks would no longer be known. | Unit Test |
| PRM-STORE-006 | An F-RAM request rejected busy shall be retried on `schedIn`, up to `BUSY_RETRY_LIMIT` times per request. Any other rejection, or running out of retries, shall fail the transfer. | The F-RAM driver is busy during its startup reads and other clients' transfers; the bound keeps a stuck driver from hanging the database's command. | Unit Test |
| PRM-STORE-007 | The component shall operate whether the F-RAM driver completes each request asynchronously or inside the call that issued it, and shall accept the database's next transfer from within the completion of the previous one. | Storage drivers differ in completion model, and `StaticPrmDb` starts its next transfer from the return of the last. | Unit Test |
| PRM-STORE-008 | The build shall fail when either bank does not hold the configured parameter store (`StaticPrmDbConfig::STORE_RECORDS` records) inside the F-RAM, or the banks overlap. | A bank layout error would silently corrupt the other bank or another F-RAM user; it must be caught before flight. | Inspection (static_assert) |

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

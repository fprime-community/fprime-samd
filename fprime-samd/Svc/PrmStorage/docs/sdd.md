# Samd21::PrmStorage

F-RAM backing store for the `Samd21.StaticPrmDb` records, with a configurable number of selectable banks.

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
| PRM-STORE-003 | A failed F-RAM request shall end the transfer with an error, issue no further requests for it, and report the direction, bank index, F-RAM offset and status of the failure. | The database fails the command and the redundant bank can fill in; the operator needs to locate the failing region of the device. | Unit Test |
| PRM-STORE-004 | The storage shall hold the store in `NUM_BANKS` banks, identified by index, packed back to back from `BASE_OFFSET`, and direct every following transfer to the bank last selected by command. Bank 0 shall be selected at boot, and the selected bank shall be reported in telemetry. | Redundant copies let a parameter survive corruption of a bank, and the project chooses how much redundancy its storage affords; which bank is loaded first is the deployment sequence's decision. | Unit Test |
| PRM-STORE-005 | A bank selection received during a transfer shall be rejected busy, and the transfer shall complete on the bank it started on. | A transfer split across banks would mix two stores, and the merge order across banks would no longer be known. | Unit Test |
| PRM-STORE-006 | An F-RAM request rejected busy shall be retried on `schedIn`, up to `BUSY_RETRY_LIMIT` times per request. Any other rejection, or running out of retries, shall fail the transfer. | The F-RAM driver is busy during its startup reads and other clients' transfers; the bound keeps a stuck driver from hanging the database's command. | Unit Test |
| PRM-STORE-007 | The component shall operate whether the F-RAM driver completes each request asynchronously or inside the call that issued it, and shall accept the database's next transfer from within the completion of the previous one. | Storage drivers differ in completion model, and a storage client may start its next transfer from the completion of the last. | Unit Test |
| PRM-STORE-008 | Each bank shall be sized to one configured parameter store (`StaticPrmDbConfig::STORE_RECORDS` records), and the build shall fail when there are no banks or the banks do not all lie inside the F-RAM. | Sizing banks from the store keeps them from overlapping; a layout past the end of the device must be caught before flight. | Unit Test, Inspection (static_assert) |
| PRM-STORE-009 | The build shall fail when one `StaticPrmDb` transfer (`StaticPrmDbConfig::SCRATCH_RECORDS` records) does not fit a single F-RAM request. | A transfer split into several chunks, with a driver that completes inside the request call, nests one request per chunk through the completion (JPL C Rule 4); one request per transfer keeps the call depth fixed. | Inspection (static_assert) |
| PRM-STORE-010 | A bank selection naming a bank that does not exist shall be rejected with a validation error and an event, and leave the selection unchanged. | The bank is a plain index on the command; an out-of-range index would address F-RAM outside the banks. | Unit Test |

## Design

Each bank holds one store of `BANK_SIZE = StaticPrmDbConfig::STORE_RECORDS * PrmRecord::SIZE` bytes, and
bank `b` starts at `BASE_OFFSET + b * BANK_SIZE`. Byte `i` of a transfer at `offset` is stored at
`BASE_OFFSET + selected * BANK_SIZE + offset + i`. One request is outstanding at a time. The completion of
one chunk issues the next. Bank 0 is selected at boot.

With the default configuration (two banks of 64 records of 12 bytes from `0x0000`):

```mermaid
---
title: "F-RAM 0x0000-0x05FF (1 unit = 64 bytes)"
---
packet-beta
0-11: "Bank 0 0x0000-0x02FF (768 B)"
12-23: "Bank 1 0x0300-0x05FF (768 B)"
```

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
| `SELECT_BANK(bank)` | command | Bank index used by the following saves and loads; `BUSY` during a transfer, `VALIDATION_ERROR` when `bank >= NUM_BANKS` |
| `ActiveBank` | telemetry | Bank of the next transfer |
| `StorageFailed` | warning high | An F-RAM request failed; the transfer returned `OTHER_ERROR` (throttled, 5) |
| `InvalidBank` | warning low | `SELECT_BANK` named a bank that does not exist |

## Configuration

Compile-time, in `samd-config/PrmStorageConfig.hpp` (override via `CONFIGURATION_OVERRIDES`):

| Constant | Meaning |
|---|---|
| `BASE_OFFSET` | F-RAM offset of bank 0; the banks are packed back to back from here |
| `NUM_BANKS` | Number of redundant banks (at least 1) |
| `BUSY_RETRY_LIMIT` | `schedIn` ticks a `BUSY`-rejected request is retried |

A bank holds one parameter store of `StaticPrmDbConfig::STORE_RECORDS * PrmRecord::SIZE` bytes
(`PrmStorageLayout::BANK_SIZE`), so the banks span `NUM_BANKS * BANK_SIZE` bytes from `BASE_OFFSET`.
`PrmStorage` static_asserts `PrmStorageLayout::banksFit`, so the build fails if there are no banks or
the last one runs past `Fram.FramConfig.Size`. It also static_asserts that one
`StaticPrmDb` transfer (`SCRATCH_RECORDS * PrmRecord::SIZE` bytes) fits `Fram.FramConfig.MaxTransfer`,
so a deployment's transfers are never chunked. The chunking still handles larger transfers from
other clients.

The component is only built when the project's library locations include fprime-devices (it uses the
`Fram` ports).

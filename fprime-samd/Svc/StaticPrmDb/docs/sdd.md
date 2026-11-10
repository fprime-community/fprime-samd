# Samd21::StaticPrmDb

A small bare-metal implementation of PrmDb

## Introduction

`StaticPrmDb` holds every parameter of a deployment in statically allocated memory and saves them to,
or loads them from, a persistent store made of self-identifying fixed-size records. The layout is
generated at build time by `tools/static-prm-db` from the deployment dictionary.

The persistent format is independent of the in-memory layout. One bad record makes only its own
parameter invalid. Records from an older dictionary still load. Two partially valid stores (e.g. the
`PrmStorage` PRIME and BACKUP banks) can be overlaid.

## Requirements

| Name | Description | Validation |
|---|---|---|
| PRM-DB-001 | `getPrm` shall return `VALID` with the value of a parameter whose records are all valid, and `INVALID` otherwise. `setPrm` shall store the value and mark all of its records valid. | Unit Test |
| PRM-DB-002 | `SAVE` followed by `LOAD REPLACE` shall restore every valid parameter. | Unit Test |
| PRM-DB-003 | A corrupted record shall invalidate only the parameter it belongs to. | Unit Test |
| PRM-DB-004 | `LOAD` shall accept records in any slot, skip blank/empty slots and records with unknown IDs, and count unknown records apart from corrupt ones. | Unit Test |
| PRM-DB-005 | `LOAD MERGE` shall take valid records only for record segments not already valid in memory. | Unit Test |
| PRM-DB-006 | `SAVE` shall write only fully valid parameters and fill the rest of the store with empty slots. | Unit Test |
| PRM-DB-007 | A failed storage read shall keep the records already loaded, still push parameters to components, and fail `LOAD`. A failed write shall fail `SAVE`. | Unit Test |
| PRM-DB-008 | During a transfer, commands shall be rejected `BUSY`, `setPrm` shall be dropped with `DroppingPrmSave`, and `getPrm` shall return `INVALID`. | Unit Test |
| PRM-DB-009 | Save and load shall work when storage completes a transfer inside the call that started it, without recursion. | Unit Test |

## Design

### Persistent record

```
[0..1]      record ID      big endian
[2..3]      CRC-16         big endian, CRC-16/CCITT-FALSE over the ID and the value segment
[4..4+V-1]  value segment  up to V bytes of the serialized value, zero padded
```

`V` is `StaticPrmDbConfig.RECORD_VALUE_SIZE` (default 8, so a 12-byte record). A parameter of `N`
bytes takes `ceil(N / V)` records. The CRC covers the ID, so a bit flip in the
ID cannot move a value onto another parameter. IDs `0x0000` (empty slot, written as all zeros) and
`0xFFFF` (erased storage) are reserved. Slots holding them are skipped without a CRC check.

The record ID is the CRC-16 of `<instance>.<param>:<type encoding>:<segment>/<V>`. It depends only on
what the record holds, not on parameter IDs or dictionary order:

- Renumbering instance base IDs, or adding or removing other parameters, keeps every stored value.
- Renaming a parameter or changing how its bytes are read (type, enum constants, struct members,
  array sizes) gives it a new ID. The old records are then skipped as *unknown*, not misread.
- Changing `V` gives every record a new ID, so a store saved with another record size loads as
  all *unknown* (and, with a different record size, mostly *corrupt*). Re-save after changing it.
- The generator fails the build when two record IDs collide or one hits a reserved ID. Rename a
  parameter to resolve it.

### In-memory store

Generated per deployment (`<Topology>StaticPrmDbAc.cpp`). There are no lookup tables: the generator
resolves every mapping at build time and emits it as code.

- `__PRM_VALUES`: a struct with one byte-array member per parameter.
- `__PRM_VALIDITY`: one bit per record. A parameter's records share one 32-bit word, so checking
  a parameter is a single `(word & mask) == mask`.
- `readParam` / `writeParam`: a `switch` on the parameter ID.
- `loadRecord`: a `switch` on the stored record ID.
- `saveRecord`: a `switch` on the record index, in parameter order.

The scratch buffer (`SCRATCH_RECORDS * PrmRecord::SIZE` bytes) is a member of the component. All
size checks that depend only on configuration live in `StaticPrmDb.hpp`; the generated file only
checks what depends on the dictionary (every record fits in `STORE_RECORDS`, and it was generated
for the configured `RECORD_VALUE_SIZE`).

Each case only passes constants (member address, size, validity word and mask, record ID) to a
shared static helper. LTO folds the helpers, so the code size per parameter is a few argument loads
and a branch.

The component itself holds no deployment knowledge. It runs the transfer state machine and the
record CRC/packing, and calls the generated functions above.

### Save

The store is `StaticPrmDbConfig::STORE_RECORDS` slots and is always written in full, one
scratch-sized transfer at a time (`writeOut(offset, buffer)`). `saveRecord` packs each record of a
valid parameter, in order. Invalid parameters are skipped and counted. The remaining slots are filled
with empty records, so records from an earlier save cannot come back.

### Load

`REPLACE` clears every validity bit first. `MERGE` keeps them. The store is read one scratch-sized
transfer at a time (`readOut(offset, buffer)`). For each record:

1. Empty or blank slots are skipped.
2. A CRC mismatch counts as *corrupt*.
3. An ID `loadRecord` does not know counts as *unknown*.
4. Otherwise the record is *valid*. `loadRecord` copies its segment into memory and sets its
   validity bit, unless the load is a `MERGE` and the bit is already set.

After the last transfer (or a failed read), the component reports `LoadedDatabase` and the
`ValidRecords`, `CorruptRecords` and `InvalidParameters` telemetry. It then goes idle and calls the
deployment's `loadParameters()`, so every component reads its parameters back through `getPrm`.
`LOAD` fails only when storage reports an error. Corrupt and unknown records only show up in the
counts.

### Transfers

There is one transfer in flight at a time. If storage completes a transfer inside the call that
started it, the completion handler only records that. `transferLoop` then issues the next transfer,
so the stack depth stays the same however many transfers the store takes.

### Ports

| Port | Kind | Meaning |
|---|---|---|
| `getPrm` / `setPrm` | guarded in / sync in | Framework parameter interface |
| `writeOut` / `writeReturnIn` | out / sync in | `Samd21.PrmTransfer(offset, buffer)` of whole records; completion with the same buffer |
| `readOut` / `readReturnIn` | out / sync in | Same, for reads |

### Commands, telemetry, events

| Item | Kind | Meaning |
|---|---|---|
| `SAVE` | command | Write every valid parameter to the store |
| `LOAD(mode)` | command | `REPLACE`: load the store into an empty database. `MERGE`: fill in from a redundant store |
| `ValidRecords` / `CorruptRecords` / `InvalidParameters` | telemetry | Result of the last load |
| `LoadedDatabase` | activity high | Load finished: valid / corrupt / unknown records, invalid parameters |
| `SavedDatabase` | activity high | Save finished: parameters written / skipped as invalid |
| `ReadFailed` | warning high | Storage failed a read; `LOAD` fails with what was read so far |
| `DroppingPrmSave` | warning high | `setPrm` during a transfer was dropped |

## Configuration

The record value size is an FPP constant, because the autocoder needs it as well as the C++. It
lives in `samd-config/Samd21StaticPrmDbConfig.fpp` (override via `CONFIGURATION_OVERRIDES`):

| Constant | Meaning |
|---|---|
| `Samd21.StaticPrmDbConfig.RECORD_VALUE_SIZE` | Value bytes per record (`PrmRecord::VALUE_SIZE`). The record is this plus 4 bytes. |

`PrmRecord::SIZE`, `VALUE_OFFSET` and the other record offsets are derived from it. A small value
wastes less padding on small parameters; a large one spends less per-record overhead on large
ones. With parameter sizes `N_i`, the store needs `sum(ceil(N_i / V)) * (V + 4)` bytes, so choose
`V` to minimize that. A parameter may take at most 32 records.

The rest is compile-time, in `samd-config/StaticPrmDbConfig.hpp` (override via
`CONFIGURATION_OVERRIDES`). Everything is in records, so it scales with the record size:

| Constant | Meaning |
|---|---|
| `SCRATCH_RECORDS` | Records per transfer. The scratch buffer is `SCRATCH_RECORDS * PrmRecord::SIZE` bytes. Keep it at or below the storage's natural request size (e.g. 5 x 12 = 60 for Fram `MaxTransfer` 64) |
| `STORE_RECORDS` | Record slots in the store. Fixed so the store does not move with the dictionary. Must hold every record of the deployment (build fails otherwise). `PrmStorage` checks the store fits its banks |

## Usage in FFB

`STARTUP` runs `SELECT_BANK PRIME` and `prm.LOAD REPLACE`, then `PRM_LOAD_BACKUP` always runs
`SELECT_BANK BACKUP` and `prm.LOAD MERGE`. Parameters PRIME did not hold a valid record for are then
filled in from BACKUP. If either load fails, the sequencer's `RUN_ON_ERROR` moves on to the next
step. `PRM_LOAD_DONE` selects `PRIME` again. To keep both banks current, a save is
`SELECT_BANK PRIME`, `prm.SAVE`, `SELECT_BANK BACKUP`, `prm.SAVE`.

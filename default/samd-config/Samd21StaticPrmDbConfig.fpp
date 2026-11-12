module Samd21 {
    module StaticPrmDbConfig {
        @ Parameter value bytes held by one persistent record. A record is this plus 4 bytes
        @ (2-byte ID, 2-byte CRC-16). A parameter takes ceil(size / RECORD_VALUE_SIZE) records,
        @ so this trades padding in records of small parameters against the per-record
        @ overhead of large ones; pick it from the deployment's parameter sizes. Records of a
        @ store saved with another value size are not loaded (their record IDs differ).
        constant RECORD_VALUE_SIZE = 8
    }
}

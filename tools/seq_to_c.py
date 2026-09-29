#!/usr/bin/env python3
"""Turn ``fprime-seqgen`` ``.bin`` sequences into linkable C byte arrays.

Two subcommands, both driven from CMake:

  ``strip``   one ``.bin`` -> one ``.c`` holding a flat command-buffer list as a
              ``const unsigned char[]`` (seqgen header/footer and the per-record
              descriptor + time tag removed).

  ``header``  a list of sequence names -> one umbrella header declaring every
              generated symbol, for the hand-written ``Action`` table to include.

Binary layout produced by ``fprime-seqgen`` (see
``fprime_gds/common/encoders/seq_writer.py``)::

    U32  size        ( == body_len + 4, i.e. body plus the trailing CRC )
    U32  num_records
    <TimeBase>                      \\  variable-width header prefix
    <FwTimeContextStoreType>        /
    U8[] body                        <-- the seqgen records
    U32  crc                         <-- trailing CRC footer

The leading ``size`` field equals ``body_len + 4`` and the whole file is
``header + body + 4``, so the (possibly variable-width) header length is
recovered without hard-coding the TimeBase / time-context widths::

    header_len = filesize - size

Each seqgen record in ``body`` is::

    U8   descriptor         ( ABSOLUTE / RELATIVE / END_OF_SEQUENCE )
    U32  seconds            \\  time tag
    U32  useconds           /
    U32  cmdSize            <-- length of the command buffer that follows
    U8[] cmd                <-- (apid, opcode, args)

Samd21::Seq does not interpret time tags: relative/absolute blocking is
expressed as explicit WAIT_TICKS / WAIT_UNTIL commands in the sequence
itself. So the emitted body drops the descriptor and time tag from every
record (and drops any END_OF_SEQUENCE record entirely), leaving a flat list
of ``(cmdSize, cmd)`` pairs -- exactly what ``Seq`` consumes.
"""

import argparse
import struct
import sys


HEADER_SIZE_FIELD = struct.Struct(">I")  # leading U32, big-endian (F Prime)
NUM_RECORDS_FIELD = struct.Struct(">I")  # second U32 of the header
CMD_SIZE_FIELD = struct.Struct(">I")  # per-record command length, big-endian
CRC_FOOTER_LEN = 4  # trailing U32 CRC

# Per-record fields dropped from the emitted body (F Prime serialization widths).
DESCRIPTOR_LEN = 1  # U8 descriptor
TIME_TAG_LEN = 8  # U32 seconds + U32 useconds
RECORD_PREFIX_LEN = DESCRIPTOR_LEN + TIME_TAG_LEN

# Descriptor value for an end-of-sequence marker (matches the old
# ActionTable::Record::END_OF_SEQUENCE). Such a record carries no further fields.
DESC_END_OF_SEQUENCE = 2


def symbol_for(name: str) -> str:
    """C identifier for a sequence's byte array, given its base name."""
    return f"fprime_seq_{name}"


def strip_header_footer(data: bytes) -> bytes:
    """Return a flat ``(cmdSize, cmd)`` command-buffer list.

    The seqgen header and CRC footer are removed, and each record's descriptor
    and time tag are stripped so only ``cmdSize`` + command bytes remain.
    """
    if len(data) < HEADER_SIZE_FIELD.size + NUM_RECORDS_FIELD.size + CRC_FOOTER_LEN:
        raise ValueError(f"sequence binary too small ({len(data)} bytes)")

    # Leading U32 = body_len + CRC_FOOTER_LEN. filesize = header + body + CRC.
    # => header_len = filesize - (body_len + CRC) = filesize - leading_u32.
    (size_field,) = HEADER_SIZE_FIELD.unpack_from(data, 0)
    header_len = len(data) - size_field
    if header_len < HEADER_SIZE_FIELD.size + NUM_RECORDS_FIELD.size:
        raise ValueError(
            f"implausible header length {header_len} "
            f"(filesize={len(data)}, size_field={size_field})"
        )
    (num_records,) = NUM_RECORDS_FIELD.unpack_from(data, HEADER_SIZE_FIELD.size)

    body = data[header_len : len(data) - CRC_FOOTER_LEN]

    out = bytearray()
    off = 0
    for rec in range(num_records):
        if off + DESCRIPTOR_LEN > len(body):
            raise ValueError(f"record {rec}: truncated before descriptor")
        descriptor = body[off]
        if descriptor == DESC_END_OF_SEQUENCE:
            # End-of-sequence marker: no time tag / command, and nothing to emit.
            off += DESCRIPTOR_LEN
            continue

        # Skip the descriptor and time tag, keep the cmdSize field + command.
        cmd_size_off = off + RECORD_PREFIX_LEN
        if cmd_size_off + CMD_SIZE_FIELD.size > len(body):
            raise ValueError(f"record {rec}: truncated before cmdSize")
        (cmd_size,) = CMD_SIZE_FIELD.unpack_from(body, cmd_size_off)

        cmd_off = cmd_size_off + CMD_SIZE_FIELD.size
        if cmd_off + cmd_size > len(body):
            raise ValueError(f"record {rec}: cmdSize {cmd_size} overruns body")

        out += CMD_SIZE_FIELD.pack(cmd_size)
        out += body[cmd_off : cmd_off + cmd_size]
        off = cmd_off + cmd_size

    if len(out) == 0:
        raise ValueError("sequence body is empty after stripping header/footer")
    return bytes(out)


def emit_c(body: bytes, symbol: str, source_name: str) -> str:
    lines = [
        "// Auto-generated by seq_to_c.py -- DO NOT EDIT.",
        f"// Source sequence: {source_name}",
        "//",
        "// Raw Samd21::Seq record body (seqgen header and CRC footer",
        "// stripped). Symbols have C linkage; see the generated header.",
        "",
        f"const unsigned char {symbol}[] = {{",
    ]
    for i in range(0, len(body), 12):
        chunk = body[i : i + 12]
        lines.append("    " + "".join(f"0x{b:02x}, " for b in chunk).rstrip())
    lines.append("};")
    lines.append(f"const unsigned int {symbol}_len = {len(body)}u;")
    lines.append("")
    return "\n".join(lines)


def emit_header(name: str) -> str:
    guard = f"SAMD21_SEQ_{name}_SEQ_H"
    lines = [
        "// Auto-generated by seq_to_c.py -- DO NOT EDIT.",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
    ]

    sym = symbol_for(name)
    lines.append(f"extern const unsigned char {sym}[];")
    lines.append(f"extern const unsigned int {sym}_len;")

    lines += [
        "",
        "#ifdef __cplusplus",
        "}  // extern \"C\"",
        "#endif",
        "",
        f"#endif  // {guard}",
        "",
    ]
    return "\n".join(lines)


def cmd_emit_c(args: argparse.Namespace) -> int:
    with open(args.input, "rb") as fd:
        data = fd.read()
    try:
        body = strip_header_footer(data)
    except ValueError as err:
        print(f"{args.input}: {err}", file=sys.stderr)
        return 1
    with open(args.output_c, "w") as fd:
        fd.write(emit_c(body, symbol_for(args.name), args.input))
    return 0


def cmd_emit_h(args: argparse.Namespace) -> int:
    with open(args.output_h, "w") as fd:
        fd.write(emit_header(args.name))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument("input", help="path to the seqgen .bin input")
    parser.add_argument("output_c", help="path to the .c file to write")
    parser.add_argument("output_h", help="path to the .h file to write")
    parser.add_argument("name", help="sequence base name (e.g. GENERIC)")

    args = parser.parse_args()
    code = cmd_emit_c(args)
    assert code == 0, "Failed to generate .c file"
    code = cmd_emit_h(args)
    assert code == 0, "Failed to generate .h file"

    return 0


if __name__ == "__main__":
    sys.exit(main())

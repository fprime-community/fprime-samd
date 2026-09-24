#!/usr/bin/env python3
"""Measure and report flash/RAM usage for a built F Prime ELF on a SAMD21 target.

Three subcommands:

  measure <elf> <out.json> [--nm <out.nm>] [--linker-script <ld>]
      Runs ``<prefix>readelf -S -W`` and ``<prefix>size -A`` and writes a JSON
      section map with flash/RAM rollups.  ``<out.json>.readelf`` and
      ``<out.json>.size`` are written alongside for the workflow artifact, and
      ``--nm`` additionally captures a demangled, size-sorted symbol dump.
      Exits non-zero if the image does not fit in the target's memory.

  render --current <cur.json> [--baseline <base.json>] [--nm <dump.nm>]
      Renders the sticky pull-request comment body as markdown.  A missing or
      unparseable baseline degrades to absolute sizes plus a "no baseline"
      callout; it is never an error.

  selftest
      Exercises the linker-script parser against this repository's own linker
      scripts plus synthetic cases.  No toolchain or build required, so CI can
      run it in the format/lint job and catch a broken parser before it silently
      reports the wrong budget.

WHERE THE MEMORY BUDGET COMES FROM
----------------------------------------------------------------------------
The flash/RAM capacities are read out of the target's linker script MEMORY
block (``--linker-script``), not hardcoded, because this script is shared by
every deployment in every repository that consumes fprime-samd and the variants
genuinely differ -- curiosity_nano and ncc are 128 KiB/16 KiB SAMD21x17, while
qtpy_m0 is a 256 KiB/32 KiB SAMD21E18A that additionally gives the first 8 KiB
of flash to a bootloader.  A hardcoded 128/16 would silently overstate usage on
one board and understate the bootloader carve-out on another.  ``--flash-bytes``
/ ``--ram-bytes`` / ``--ram-origin`` override individually; with neither the
SAMD21x17 values are assumed, which is what the bare defaults used to be.

HOW SECTIONS ARE CLASSIFIED
----------------------------------------------------------------------------
Section classification is driven by ELF flags and types, not by section names:
``flash_without_bootloader.ld`` names the initialised-data output section
``.relocate``, not ``.data``, so a name-based parser reports 0 bytes of data
forever and never notices.  GNU ld also stamps ``.relocate`` with section type
``REL`` (a side effect of the ``.rel*`` name convention), so the type is only
consulted to spot ``NOBITS``.

  * ALLOC and not NOBITS  -> occupies flash (present in a PT_LOAD FileSiz)
  * ALLOC and addr in RAM -> occupies RAM
  * not ALLOC             -> .debug_*, .comment, .symtab, .ARM.attributes:
                             carried in the ELF, free on the target

Initialised data is charged to *both* regions: ``.relocate : AT (__etext)``
stores it in flash and ``Reset_Handler`` copies it into RAM at boot.
"""

import argparse
import json
import os
import re
import subprocess
import sys

# SAMD21G17A/D, per cmake/toolchain/samd21/curiosity_nano/linker_scripts/
# flash_without_bootloader.ld:
#   FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x00020000
#   RAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 0x00004000
# Used only when no --linker-script and no explicit override is given, so that
# callers predating the linker-script support keep their previous behaviour.
DEFAULT_FLASH_BYTES = 0x20000
DEFAULT_RAM_BYTES = 0x4000
DEFAULT_RAM_ORIGIN = 0x20000000
DEFAULT_LABEL = "SAMD21G17A/D"

# GitHub hard-caps an issue/pull-request comment body at 65536 characters.
COMMENT_LIMIT = 65536
# Upper bound on the collapsed nm dump.  The real budget is computed from the
# rendered size of everything else, so the tables are never the thing that gets
# cut; this cap only keeps the comment from becoming unreadably long.
NM_BUDGET_MAX = 40000
# Head-room reserved for the truncation footer and the closing fence/details.
NM_FOOTER_RESERVE = 1024

# readelf -S -W row, e.g.
#   [ 4] .relocate         REL             20000000 020000 0000ac 08  WA  0   0  4
# The flags column is empty for non-ALLOC sections, hence the optional group.
_SECTION_ROW = re.compile(
    r"^\s*\[\s*\d+\]\s+(?P<name>\S*)\s+(?P<type>[A-Z][A-Z0-9_]*)"
    r"\s+(?P<addr>[0-9a-fA-F]+)\s+(?P<off>[0-9a-fA-F]+)\s+(?P<size>[0-9a-fA-F]+)"
    r"\s+(?P<es>[0-9a-fA-F]+)(?P<flags>(?:\s+[A-Za-z]+)?)"
    r"\s+\d+\s+\d+\s+\d+\s*$"
)

# One signed term of a MEMORY-block expression: an optional sign, an integer in
# hex or decimal, and GNU ld's optional K/M scale suffix.
_LD_TERM = re.compile(
    r"\s*(?P<sign>[+-]?)\s*(?P<num>0[xX][0-9a-fA-F]+|\d+)\s*(?P<suffix>[KMkm]?)"
)

# A MEMORY-block region line, e.g.
#   FLASH (rx) : ORIGIN = 0x00000000+0x2000, LENGTH = 0x00040000-0x2000
# The attribute parenthesis is optional, and ld accepts `org`/`len` as aliases
# for ORIGIN/LENGTH.
_MEMORY_REGION = re.compile(
    r"^\s*(?P<name>[A-Za-z_][A-Za-z0-9_.]*)\s*(?:\([^)]*\))?\s*:\s*"
    r"(?:ORIGIN|org)\s*=\s*(?P<origin>[^,]+),\s*"
    r"(?:LENGTH|len|l)\s*=\s*(?P<length>.+?)\s*$",
    re.IGNORECASE,
)


def tool(name, prefix=None):
    """Return the binutils executable name for ``name``, honouring the prefix."""
    if prefix is None:
        prefix = os.environ.get("ARM_TOOL_PREFIX", "arm-none-eabi-")
    return prefix + name


def _eval_ld_expr(expr, where):
    """Evaluate the restricted integer expression GNU ld allows in MEMORY.

    Supports a sum of signed hex/decimal terms with optional ``K``/``M`` scale
    suffixes, which covers everything the fprime-samd linker scripts use
    (``0x00040000-0x2000`` for the qtpy bootloader carve-out) and the usual
    ``128K`` idiom.  Deliberately NOT ``eval``: the point of a real parser here
    is that anything it does not understand raises instead of quietly producing
    a wrong memory budget, which would make every size report a lie.
    """
    cleaned = expr.strip()
    if not cleaned:
        raise ValueError(f"{where}: empty expression")
    total = 0
    pos = 0
    first = True
    while pos < len(cleaned):
        match = _LD_TERM.match(cleaned, pos)
        if match is None:
            raise ValueError(f"{where}: cannot parse {expr!r} at offset {pos}")
        sign_text = match.group("sign")
        if not first and not sign_text:
            # Two adjacent terms with no operator, e.g. "0x100 0x200".  Refuse
            # rather than pick one.
            raise ValueError(f"{where}: missing operator in {expr!r} at offset {pos}")
        digits = match.group("num")
        value = int(digits, 16) if digits[:2].lower() == "0x" else int(digits, 10)
        suffix = match.group("suffix").upper()
        if suffix == "K":
            value *= 1024
        elif suffix == "M":
            value *= 1024 * 1024
        total += -value if sign_text == "-" else value
        pos = match.end()
        first = False
    return total


def parse_linker_memory(path):
    """Parse a linker script's MEMORY block into ``{region: {origin, length}}``.

    Only the MEMORY block is interpreted; ``INCLUDE`` directives and symbol
    references are not followed, so a script that computes a region size from a
    PROVIDEd symbol will raise rather than be guessed at.
    """
    with open(path, encoding="utf-8", errors="replace") as handle:
        text = handle.read()

    # Strip /* ... */ comments first; qtpy_m0 has one inside the MEMORY block.
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)

    start = re.search(r"\bMEMORY\b\s*\{", text)
    if start is None:
        raise ValueError(f"{path}: no MEMORY block found")
    depth = 0
    body_start = text.index("{", start.start())
    end = None
    for index in range(body_start, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                end = index
                break
    if end is None:
        raise ValueError(f"{path}: unterminated MEMORY block")

    regions = {}
    for line in text[body_start + 1 : end].splitlines():
        if not line.strip():
            continue
        match = _MEMORY_REGION.match(line)
        if match is None:
            raise ValueError(f"{path}: cannot parse MEMORY region line {line.strip()!r}")
        name = match.group("name").upper()
        regions[name] = {
            "origin": _eval_ld_expr(match.group("origin"), f"{path} {name} ORIGIN"),
            "length": _eval_ld_expr(match.group("length"), f"{path} {name} LENGTH"),
        }
    if not regions:
        raise ValueError(f"{path}: MEMORY block declares no regions")
    return regions


def resolve_memory(
    linker_script=None,
    flash_bytes=None,
    ram_bytes=None,
    ram_origin=None,
    flash_region="FLASH",
    ram_region="RAM",
    label=None,
):
    """Work out the target's memory budget and where it came from.

    Precedence: explicit ``--flash-bytes``/``--ram-bytes``/``--ram-origin`` win
    over the linker script, which wins over the SAMD21x17 defaults.  Mixing is
    allowed on purpose -- overriding just ``--flash-bytes`` to model a
    bootloader reservation while taking RAM from the script is a reasonable
    thing to want.
    """
    source = "SAMD21x17 built-in defaults"
    flash = DEFAULT_FLASH_BYTES
    ram = DEFAULT_RAM_BYTES
    origin = DEFAULT_RAM_ORIGIN

    if linker_script:
        regions = parse_linker_memory(linker_script)
        missing = [
            name for name in (flash_region, ram_region) if name.upper() not in regions
        ]
        if missing:
            raise ValueError(
                f"{linker_script}: MEMORY block has no region(s) {', '.join(missing)};"
                f" found {', '.join(sorted(regions))}."
                " Use --flash-region/--ram-region, or give explicit --flash-bytes"
                " and --ram-bytes."
            )
        flash = regions[flash_region.upper()]["length"]
        ram = regions[ram_region.upper()]["length"]
        origin = regions[ram_region.upper()]["origin"]
        source = f"{os.path.basename(linker_script)} MEMORY block"

    overridden = []
    if flash_bytes is not None:
        flash = flash_bytes
        overridden.append("flash")
    if ram_bytes is not None:
        ram = ram_bytes
        overridden.append("ram")
    if ram_origin is not None:
        origin = ram_origin
        overridden.append("ram origin")
    if overridden:
        source = f"{source}, {'/'.join(overridden)} overridden on the command line"

    if flash <= 0 or ram <= 0:
        raise ValueError(
            f"non-positive memory budget (flash={flash}, ram={ram}) from {source}"
        )

    return {
        "flash_capacity": flash,
        "ram_capacity": ram,
        "ram_origin": origin,
        "budget_source": source,
        "label": label or DEFAULT_LABEL,
    }


def parse_readelf(text):
    """Parse ``readelf -S -W`` output into ``{name: {size, addr, type, alloc}}``."""
    sections = {}
    for line in text.splitlines():
        match = _SECTION_ROW.match(line)
        if match is None or not match.group("name"):
            continue
        sections[match.group("name")] = {
            "size": int(match.group("size"), 16),
            "addr": int(match.group("addr"), 16),
            "type": match.group("type"),
            "alloc": "A" in match.group("flags"),
        }
    return sections


def rollup(sections, ram_origin=DEFAULT_RAM_ORIGIN, ram_bytes=DEFAULT_RAM_BYTES):
    """Sum the flash and RAM cost of ``sections`` by ELF flags, not by name."""
    flash = 0
    ram = 0
    for info in sections.values():
        if not info["alloc"]:
            continue
        if info["type"] != "NOBITS":
            flash += info["size"]
        if ram_origin <= info["addr"] < ram_origin + ram_bytes:
            ram += info["size"]
    return flash, ram


def measure(elf, out_path, nm_path=None, prefix=None, memory=None):
    """Measure ``elf``, write the JSON report, and return a process exit code."""
    if memory is None:
        memory = resolve_memory()
    flash_capacity = memory["flash_capacity"]
    ram_capacity = memory["ram_capacity"]

    readelf = subprocess.run(
        [tool("readelf", prefix), "-S", "-W", elf],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    size_a = subprocess.run(
        [tool("size", prefix), "-A", elf], check=True, capture_output=True, text=True
    ).stdout

    sections = parse_readelf(readelf)
    if not sections:
        print(f"error: no section headers parsed from {elf}", file=sys.stderr)
        return 1

    flash, ram = rollup(sections, memory["ram_origin"], ram_capacity)

    def section_size(name):
        return sections.get(name, {}).get("size", 0)

    report = {
        "elf": os.path.basename(elf),
        "all_sections": {name: info["size"] for name, info in sections.items()},
        "alloc_sections": {
            name: info["size"] for name, info in sections.items() if info["alloc"]
        },
        # `.relocate` is this linker script's spelling of `.data`; accept either.
        "data": section_size(".data") + section_size(".relocate"),
        "text": section_size(".text"),
        "bss": section_size(".bss"),
        "flash": flash,
        "ram": ram,
        "flash_capacity": flash_capacity,
        "ram_capacity": ram_capacity,
        # Provenance, so a surprising report can be traced to the budget it used
        # rather than only to the numbers it produced.
        "ram_origin": memory["ram_origin"],
        "budget_source": memory["budget_source"],
        "label": memory["label"],
    }

    with open(out_path, "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=True)
    with open(out_path + ".readelf", "w", encoding="utf-8") as handle:
        handle.write(readelf)
    with open(out_path + ".size", "w", encoding="utf-8") as handle:
        handle.write(size_a)

    if nm_path is not None:
        # --reverse-sort puts the largest symbols first, which is what matters
        # when only part of the dump survives truncation in the comment.
        nm_out = subprocess.run(
            [
                tool("nm", prefix),
                "--print-size",
                "--size-sort",
                "--reverse-sort",
                "--demangle",
                elf,
            ],
            check=True,
            capture_output=True,
            text=True,
        ).stdout
        with open(nm_path, "w", encoding="utf-8") as handle:
            handle.write(nm_out)

    print(
        f"{report['elf']}: text={report['text']} data={report['data']}"
        f" bss={report['bss']}"
    )
    print(f"  budget from {memory['budget_source']} ({memory['label']})")
    print(
        f"  flash {flash}/{flash_capacity} ({flash / flash_capacity * 100:.2f}%)"
        f"  ram {ram}/{ram_capacity} ({ram / ram_capacity * 100:.2f}%)"
    )
    if flash > flash_capacity or ram > ram_capacity:
        # The link should already have failed on a region overflow; this is a
        # belt-and-braces gate so a budget blow-out is a red check either way.
        print("error: image does not fit in the target's memory", file=sys.stderr)
        return 1
    return 0


def load_report(path):
    """Load a measurement JSON, returning ``None`` if it is absent or broken."""
    if not path or not os.path.isfile(path):
        return None
    try:
        with open(path, encoding="utf-8") as handle:
            report = json.load(handle)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return None
    return report if isinstance(report, dict) and "flash" in report else None


def format_delta(current, baseline):
    """Render a signed byte delta, or ``n/a`` when there is nothing to compare."""
    if current is None or baseline is None:
        return "n/a"
    diff = current - baseline
    if diff == 0:
        return "no change"
    percent = f" ({diff / baseline * 100:+.2f}%)" if baseline else ""
    return f"**{diff:+,} B**{percent}"


def bar(used, capacity, width=30):
    """Render a fixed-width ASCII usage bar."""
    filled = min(width, round(used / capacity * width)) if capacity else 0
    return "#" * filled + "." * (width - filled)


def read_nm(path):
    """Read an nm dump into a list of lines; missing or unreadable means empty."""
    if not path:
        return []
    try:
        with open(path, encoding="utf-8", errors="replace") as handle:
            return handle.read().splitlines()
    except OSError:
        return []


def truncate_nm(lines, budget):
    """Take whole lines from ``lines`` while they fit in ``budget`` characters."""
    kept = []
    used = 0
    for line in lines:
        if used + len(line) + 1 > budget:
            return kept, True
        kept.append(line)
        used += len(line) + 1
    return kept, False


def _kib(value):
    """Format a byte count as KiB, without a trailing ``.0``."""
    kib = value / 1024
    return f"{kib:.0f} KiB" if abs(kib - round(kib)) < 0.005 else f"{kib:.2f} KiB"


def render(
    current,
    baseline=None,
    nm_lines=(),
    head_sha="",
    base_sha="",
    baseline_origin="unavailable",
    marker="samd21-size-report",
    title=None,
    baseline_label="`main`",
):
    """Render the pull-request comment body for ``current`` vs ``baseline``."""
    out = []
    # Marker used by the reporter workflow to find and edit its own comment.
    # Deployments sharing one repository MUST pass distinct markers, or their
    # comments overwrite each other.
    out.append(f"<!-- {marker} -->")
    out.append(f"## {title or 'SAMD21 flash / RAM size report'}")
    out.append("")
    # Capacities and the part label come from the measurement, so this line stays
    # correct across board variants instead of asserting one part's numbers.
    out.append(
        f"`{current['elf']}` at `{head_sha[:8] or 'unknown'}` --"
        f" {current.get('label', DEFAULT_LABEL)},"
        f" {_kib(current['flash_capacity'])} flash /"
        f" {_kib(current['ram_capacity'])} RAM"
    )
    out.append("")

    if baseline is None:
        out.append("> [!NOTE]")
        out.append(
            f"> No {baseline_label} baseline was available for this run, so only absolute"
            " sizes are shown. That is expected on the first pull request, on a cold"
            " cache, or when the baseline build could not be completed (for example when"
            " this pull request bumps a submodule or a pinned autocoder version). The"
            " build itself still passed or failed on its own merits."
        )
        out.append("")
    elif baseline.get("flash_capacity") != current.get("flash_capacity") or baseline.get(
        "ram_capacity"
    ) != current.get("ram_capacity"):
        # A capacity change means the two builds were measured against different
        # memory budgets, so the percentages are not comparable even though the
        # byte deltas still are.  Say so rather than quietly mixing them.
        out.append("> [!WARNING]")
        out.append(
            "> The baseline was measured against a different memory budget"
            f" ({_kib(baseline.get('flash_capacity', 0))} flash /"
            f" {_kib(baseline.get('ram_capacity', 0))} RAM). Byte deltas below are still"
            " meaningful; the percentage-of-capacity figures are not comparable."
        )
        out.append("")

    out.append(f"| Section | This PR | {baseline_label} | Delta | Consumes |")
    out.append("| :-- | --: | --: | --: | :-- |")
    for label, key, consumes in (
        ("`.text`", "text", "flash"),
        ("`.data` (emitted as `.relocate`)", "data", "flash **and** RAM"),
        ("`.bss`", "bss", "RAM"),
    ):
        base_value = baseline.get(key) if baseline else None
        base_cell = f"`{base_value:,}` B" if base_value is not None else "--"
        out.append(
            f"| {label} | `{current[key]:,}` B | {base_cell} |"
            f" {format_delta(current[key], base_value)} | {consumes} |"
        )
    out.append("")

    out.append("### Budget")
    out.append("")
    out.append("| Region | Used | Capacity | Free | Used | Delta |")
    out.append("| :-- | --: | --: | --: | --: | --: |")
    for label, key, capacity_key in (
        ("Flash", "flash", "flash_capacity"),
        ("RAM (static)", "ram", "ram_capacity"),
    ):
        capacity = current[capacity_key]
        used = current[key]
        base_value = baseline.get(key) if baseline else None
        out.append(
            f"| {label} | `{used:,}` B ({used / 1024:.2f} KiB) | `{capacity:,}` B |"
            f" `{capacity - used:,}` B | {used / capacity * 100:.2f}% |"
            f" {format_delta(used, base_value)} |"
        )
    out.append("")
    out.append("```text")
    for label, key, capacity_key in (
        ("flash", "flash", "flash_capacity"),
        ("ram  ", "ram", "ram_capacity"),
    ):
        percent = current[key] / current[capacity_key] * 100
        out.append(
            f"{label} [{bar(current[key], current[capacity_key])}] {percent:5.1f}%"
        )
    out.append("```")
    out.append("")
    out.append(
        "Flash is every `ALLOC` section that is not `NOBITS`; RAM is every `ALLOC`"
        f" section placed at `0x{current.get('ram_origin', DEFAULT_RAM_ORIGIN):08X}`."
        " Initialised data is charged to **both** regions: it is stored in flash"
        " (`.relocate : AT (__etext)`) and copied into RAM by `Reset_Handler`. The heap"
        " and the stack are carved out of whatever RAM is left at runtime and are not"
        " counted here -- the linker script only asserts `__StackLimit >= __HeapLimit`"
        " at link time. Non-`ALLOC` sections (`.debug_*`, `.comment`, `.symtab`,"
        " `.ARM.attributes`) cost nothing on the target."
    )
    out.append("")

    out.append("<details>")
    out.append(
        "<summary>Per-section breakdown (<code>ALLOC</code> sections only)</summary>"
    )
    out.append("")
    out.append(f"| Section | This PR | {baseline_label} | Delta |")
    out.append("| :-- | --: | --: | --: |")
    base_alloc = (baseline or {}).get("alloc_sections", {})
    for name in sorted(set(current["alloc_sections"]) | set(base_alloc)):
        current_value = current["alloc_sections"].get(name)
        base_value = base_alloc.get(name)
        current_cell = f"`{current_value:,}`" if current_value is not None else "--"
        base_cell = f"`{base_value:,}`" if base_value is not None else "--"
        out.append(
            f"| `{name}` | {current_cell} | {base_cell} |"
            f" {format_delta(current_value, base_value)} |"
        )
    out.append("")
    out.append("</details>")
    out.append("")

    # Everything above, plus the footer, is fixed cost; the nm dump gets what is
    # left of GitHub's comment budget, capped for readability.
    footer = (
        f"<sub>Budget: {current.get('budget_source', 'built-in defaults')}."
        f" Baseline: {baseline_origin}"
        f"{f' (`{base_sha[:8]}`)' if base_sha else ''}."
        " This comment is updated in place on every push.</sub>"
    )
    fixed = len("\n".join(out + [footer]))
    budget = min(NM_BUDGET_MAX, max(0, COMMENT_LIMIT - fixed - NM_FOOTER_RESERVE))
    kept, truncated = truncate_nm(list(nm_lines), budget)

    out.append("<details>")
    out.append(
        "<summary>Symbol sizes -- <code>nm --print-size --size-sort --reverse-sort"
        f" --demangle</code> ({len(kept):,} of {len(nm_lines):,} symbols, largest"
        " first)</summary>"
    )
    out.append("")
    out.append("```text")
    out.extend(kept)
    if truncated:
        out.append("")
        out.append(
            f"[truncated: {len(nm_lines) - len(kept):,} smaller symbols omitted to stay"
            " under GitHub's 65536-character comment limit. The complete dump is in the"
            " `size-report` workflow artifact.]"
        )
    out.append("```")
    out.append("")
    out.append("</details>")
    out.append("")
    out.append(footer)

    body = "\n".join(out)
    if len(body) > COMMENT_LIMIT:  # belt and braces
        body = body[: COMMENT_LIMIT - 256].rsplit("\n", 1)[0] + "\n```\n\n</details>\n"
    return body


def _memory_from_args(args):
    """Build the memory budget described by the shared --flash/--ram arguments."""
    return resolve_memory(
        linker_script=args.linker_script or None,
        flash_bytes=args.flash_bytes,
        ram_bytes=args.ram_bytes,
        ram_origin=args.ram_origin,
        flash_region=args.flash_region,
        ram_region=args.ram_region,
        label=args.label or None,
    )


def _cmd_measure(args):
    try:
        memory = _memory_from_args(args)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return measure(
        args.elf,
        args.output,
        nm_path=args.nm,
        prefix=args.tool_prefix,
        memory=memory,
    )


def _cmd_render(args):
    current = load_report(args.current)
    if current is None:
        print(
            f"error: current measurement {args.current!r} is missing or unparseable",
            file=sys.stderr,
        )
        return 1
    body = render(
        current,
        baseline=load_report(args.baseline),
        nm_lines=read_nm(args.nm),
        head_sha=args.head_sha,
        base_sha=args.base_sha,
        baseline_origin=args.baseline_origin,
        marker=args.marker,
        title=args.title or None,
        baseline_label=args.baseline_label,
    )
    with open(args.output, "w", encoding="utf-8") as handle:
        handle.write(body)
    print(f"rendered {len(body):,} characters (GitHub limit {COMMENT_LIMIT:,})")
    return 0


def _cmd_selftest(args):
    """Check the linker-script parser against real and synthetic inputs."""
    failures = []

    def check(what, got, want):
        if got != want:
            failures.append(f"{what}: expected {want!r}, got {got!r}")

    # Expression evaluator, including the qtpy bootloader carve-out form and the
    # K/M suffixes ld allows.
    check("0x20000", _eval_ld_expr("0x20000", "t"), 0x20000)
    check("bootloader carve-out", _eval_ld_expr("0x00040000-0x2000", "t"), 0x3E000)
    check("origin offset", _eval_ld_expr("0x00000000+0x2000", "t"), 0x2000)
    check("128K", _eval_ld_expr("128K", "t"), 131072)
    check("1M", _eval_ld_expr(" 1M ", "t"), 1048576)
    check("decimal", _eval_ld_expr("4096", "t"), 4096)
    check("three terms", _eval_ld_expr("16K + 0x100 - 256", "t"), 16384 + 256 - 256)
    for bad in ("", "0x100 0x200", "FLASH", "0x1 +"):
        try:
            _eval_ld_expr(bad, "t")
        except ValueError:
            pass
        else:
            failures.append(f"{bad!r}: expected a ValueError, got none")

    # Real linker scripts, located relative to this file so the check works from
    # any working directory.
    here = os.path.dirname(os.path.abspath(__file__))
    variants = os.path.join(here, "..", "..", "cmake", "toolchain", "samd21")
    expected = {
        "curiosity_nano/linker_scripts/flash_without_bootloader.ld": (
            0x20000,
            0x4000,
            0x20000000,
        ),
        "ncc/linker_scripts/flash_without_bootloader.ld": (
            0x20000,
            0x4000,
            0x20000000,
        ),
        # 256 KiB part, less the 8 KiB UF2 bootloader.
        "qtpy_m0/linker_scripts/flash_with_bootloader.ld": (
            0x40000 - 0x2000,
            0x8000,
            0x20000000,
        ),
    }
    for relative, (flash, ram, origin) in expected.items():
        path = os.path.normpath(os.path.join(variants, relative))
        if not os.path.isfile(path):
            failures.append(f"{relative}: linker script not found at {path}")
            continue
        try:
            memory = resolve_memory(linker_script=path)
        except (OSError, ValueError) as error:
            failures.append(f"{relative}: {error}")
            continue
        check(f"{relative} flash", memory["flash_capacity"], flash)
        check(f"{relative} ram", memory["ram_capacity"], ram)
        check(f"{relative} ram origin", memory["ram_origin"], origin)

    # Overrides must beat the script, and the provenance string must say so.
    try:
        path = os.path.normpath(
            os.path.join(
                variants, "curiosity_nano/linker_scripts/flash_without_bootloader.ld"
            )
        )
        memory = resolve_memory(linker_script=path, flash_bytes=1024)
        check("override flash", memory["flash_capacity"], 1024)
        check("override keeps ram", memory["ram_capacity"], 0x4000)
        if "overridden" not in memory["budget_source"]:
            failures.append("override provenance not recorded in budget_source")
    except (OSError, ValueError) as error:
        failures.append(f"override case: {error}")

    # A missing region must raise a message naming the regions that do exist,
    # rather than defaulting to a wrong budget.
    try:
        resolve_memory(linker_script=path, flash_region="NOPE")
    except ValueError as error:
        if "NOPE" not in str(error):
            failures.append(f"missing-region error does not name it: {error}")
    else:
        failures.append("missing region did not raise")

    for failure in failures:
        print(f"FAIL {failure}", file=sys.stderr)
    if failures:
        print(f"{len(failures)} self-test failure(s)", file=sys.stderr)
        return 1
    print("size_report.py self-test: all checks passed")
    return 0


def _add_memory_arguments(parser):
    """Add the memory-budget arguments shared by the measuring subcommands."""
    group = parser.add_argument_group("target memory budget")
    group.add_argument(
        "--linker-script",
        default=os.environ.get("LINKER_SCRIPT", ""),
        help="read flash/RAM capacity from this linker script's MEMORY block",
    )
    group.add_argument(
        "--flash-bytes",
        type=lambda value: int(value, 0),
        default=None,
        help="override flash capacity (accepts 0x... )",
    )
    group.add_argument(
        "--ram-bytes",
        type=lambda value: int(value, 0),
        default=None,
        help="override RAM capacity (accepts 0x... )",
    )
    group.add_argument(
        "--ram-origin",
        type=lambda value: int(value, 0),
        default=None,
        help="override the RAM region base address (accepts 0x... )",
    )
    group.add_argument(
        "--flash-region",
        default="FLASH",
        help="MEMORY region name holding code (default: FLASH)",
    )
    group.add_argument(
        "--ram-region",
        default="RAM",
        help="MEMORY region name holding data (default: RAM)",
    )
    group.add_argument(
        "--label",
        default=os.environ.get("TARGET_LABEL", ""),
        help=f"part name shown in the report (default: {DEFAULT_LABEL})",
    )


def build_parser():
    """Build the argument parser for the command-line entry point."""
    parser = argparse.ArgumentParser(
        prog="size_report.py",
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    measure_parser = subparsers.add_parser(
        "measure", help="measure a built ELF into a JSON report"
    )
    measure_parser.add_argument("elf", help="path to the linked .elf")
    measure_parser.add_argument("output", help="path of the JSON report to write")
    measure_parser.add_argument(
        "--nm", default=None, help="also write a demangled, size-sorted symbol dump here"
    )
    measure_parser.add_argument(
        "--tool-prefix",
        default=None,
        help="binutils prefix (default: $ARM_TOOL_PREFIX or arm-none-eabi-)",
    )
    _add_memory_arguments(measure_parser)
    measure_parser.set_defaults(func=_cmd_measure)

    render_parser = subparsers.add_parser(
        "render", help="render the pull-request comment markdown"
    )
    render_parser.add_argument(
        "--current",
        default=os.environ.get("CURRENT_JSON", "size/current.json"),
        help="measurement JSON for this build",
    )
    render_parser.add_argument(
        "--baseline",
        default=os.environ.get("BASELINE_JSON", ""),
        help="measurement JSON for the baseline; missing or broken means no baseline",
    )
    render_parser.add_argument(
        "--nm", default=os.environ.get("NM_TXT", ""), help="symbol dump to embed"
    )
    render_parser.add_argument(
        "--output",
        default=os.environ.get("OUTPUT", "size-report.md"),
        help="markdown file to write",
    )
    render_parser.add_argument("--head-sha", default=os.environ.get("HEAD_SHA", ""))
    render_parser.add_argument("--base-sha", default=os.environ.get("BASE_SHA", ""))
    render_parser.add_argument(
        "--baseline-origin",
        default=os.environ.get("BASELINE_ORIGIN", "unavailable"),
        help="human-readable provenance of the baseline measurement",
    )
    render_parser.add_argument(
        "--marker",
        default=os.environ.get("REPORT_MARKER", "samd21-size-report"),
        help=(
            "HTML comment marker the reporter uses to find and edit its own comment."
            " Repositories with more than one deployment MUST use a distinct marker per"
            " deployment or the comments overwrite each other."
        ),
    )
    render_parser.add_argument(
        "--title",
        default=os.environ.get("REPORT_TITLE", ""),
        help="heading for the comment (default: SAMD21 flash / RAM size report)",
    )
    render_parser.add_argument(
        "--baseline-label",
        default=os.environ.get("BASELINE_LABEL", "`main`"),
        help="what to call the baseline column (default: `main`)",
    )
    render_parser.set_defaults(func=_cmd_render)

    selftest_parser = subparsers.add_parser(
        "selftest", help="check the linker-script parser; needs no toolchain"
    )
    selftest_parser.set_defaults(func=_cmd_selftest)
    return parser


def main(argv=None):
    """Command-line entry point; returns a process exit code."""
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

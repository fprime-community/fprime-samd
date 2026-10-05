#!/usr/bin/env python3
"""Turn a ctest JUnit report into per-module unit-test console-log pages.

The coverage site's "Unit tests" column links to the console output of the run that
produced each module's coverage. That output comes from the single whole-library
`ctest` invocation -- the one that gates the build -- captured with
`ctest --output-junit`, which records each test's stdout/stderr in <system-out>.

Note that the report must be produced with the output-size limits raised.  ctest
truncates *passing* tests' output to 1 KiB by default and substitutes "[This part of
the test output was removed...]", which discards nearly all of a gtest run.  The
unit-tests action's `output-size-limit` input handles this.

Output, written next to each module's `coverage/` directory so the site publisher
mirrors it the same way:

    <modules-root>/tests/index.html              every test in the run
    <modules-root>/tests/console.txt             the same, as plain text
    <modules-root>/tests/summary.json
    <modules-root>/<module>/tests/index.html     that module's tests
    <modules-root>/<module>/tests/console.txt
    <modules-root>/<module>/tests/summary.json

Tests are attributed to modules by exact target name rather than by pattern-matching,
because a module directory containing an underscore would make the reverse mapping
ambiguous.  F Prime derives a module's name from its path below the library root, so
`Drv/GpioDriver` builds `fprime-samd_Drv_GpioDriver_ut_exe`.  Anything in the report
that does not map to a discovered module is reported as a warning and still appears on
the whole-run page, so a renamed target shows up as a visible gap rather than silently
vanishing from the site.
"""

from __future__ import annotations

import argparse
import html
import json
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

#: Per-test cap on what gets written into a page. The JUnit report is already capped
#: by ctest; this is a second guard so one pathological test cannot produce a
#: multi-megabyte HTML page that no browser will open.
DEFAULT_MAX_BYTES = 1_000_000

STATUS_PASSED = "passed"
STATUS_FAILED = "failed"
STATUS_SKIPPED = "skipped"


@dataclass
class Case:
    name: str
    status: str
    duration: float
    output: str

    def to_entry(self) -> dict:
        return {"name": self.name, "status": self.status, "duration": round(self.duration, 3)}


@dataclass
class Bucket:
    """The tests attributed to one destination page."""

    label: str
    cases: list[Case] = field(default_factory=list)

    @property
    def counts(self) -> dict:
        return {
            "tests": len(self.cases),
            STATUS_PASSED: sum(1 for c in self.cases if c.status == STATUS_PASSED),
            STATUS_FAILED: sum(1 for c in self.cases if c.status == STATUS_FAILED),
            STATUS_SKIPPED: sum(1 for c in self.cases if c.status == STATUS_SKIPPED),
        }

    @property
    def status(self) -> str:
        if any(c.status == STATUS_FAILED for c in self.cases):
            return STATUS_FAILED
        if self.cases and all(c.status == STATUS_SKIPPED for c in self.cases):
            return STATUS_SKIPPED
        return STATUS_PASSED

    def to_summary(self) -> dict:
        counts = self.counts
        return {
            "status": self.status,
            "tests": counts["tests"],
            "passed": counts[STATUS_PASSED],
            "failed": counts[STATUS_FAILED],
            "skipped": counts[STATUS_SKIPPED],
            "duration": round(sum(c.duration for c in self.cases), 3),
            "report": "tests/index.html",
            "cases": [c.to_entry() for c in self.cases],
        }


def parse_junit(path: Path) -> list[Case]:
    """Read every <testcase> in a ctest JUnit report.

    ctest marks a test with status="run" when it ran, "fail" when it failed, and
    "notrun"/"disabled" when it was skipped; a <failure> child is the authoritative
    failure marker.
    """
    cases: list[Case] = []
    root = ET.parse(path).getroot()
    for element in root.iter("testcase"):
        if element.find("failure") is not None or element.get("status") == "fail":
            status = STATUS_FAILED
        elif element.find("skipped") is not None or element.get("status") in (
            "notrun",
            "disabled",
        ):
            status = STATUS_SKIPPED
        else:
            status = STATUS_PASSED
        try:
            duration = float(element.get("time") or 0.0)
        except ValueError:
            duration = 0.0
        output = element.findtext("system-out") or ""
        failure = element.find("failure")
        if failure is not None:
            # Worth appending when ctest knows something the test's own output does
            # not -- a timeout, a missing executable, an unmatched required regex. For
            # an ordinary non-zero exit ctest just says "Failed", which adds a
            # confusing line to a log that already ends in the real assertion, so
            # that case and anything already quoted in the output are dropped.
            detail = ((failure.get("message") or "") + " " + (failure.text or "")).strip()
            if detail and detail.lower() != "failed" and detail not in output:
                output = f"{output}\n--- ctest reported: {detail} ---\n"
        cases.append(Case(element.get("name") or "<unnamed>", status, duration, output))
    return cases


def target_name(module: str, prefix: str, suffix: str) -> str:
    """The ctest test name F Prime registers for a module's unit test."""
    stem = module.replace("/", "_")
    return f"{prefix}_{stem}{suffix}" if prefix else f"{stem}{suffix}"


# --------------------------------------------------------------------------- #
# Rendering                                                                   #
# --------------------------------------------------------------------------- #

CSS = """* { box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  margin: 0; padding: 1.5rem; color: #1f2328; background: #fff;
}
h1 { margin: 0 0 0.25rem 0; font-size: 1.25rem; }
h1 code { font-size: 1rem; }
.meta { color: #57606a; font-size: 0.9rem; margin-bottom: 1rem; }
a { color: #0969da; text-decoration: none; }
a:hover { text-decoration: underline; }
.pill {
  display: inline-block; padding: 0.05rem 0.55rem; border-radius: 2em;
  font-size: 0.78rem; font-weight: 600; border: 1px solid transparent;
}
.passed { background: #dafbe1; color: #0a5122; border-color: #4ac26b; }
.failed { background: #ffebe9; color: #a40e26; border-color: #ff8182; }
.skipped { background: #f6f8fa; color: #57606a; border-color: #d0d7de; }
details { border: 1px solid #d0d7de; border-radius: 6px; margin-bottom: 0.5rem; }
details > summary {
  cursor: pointer; padding: 0.5rem 0.75rem; font-weight: 600;
  display: flex; gap: 0.6rem; align-items: center; list-style: none;
}
details > summary::-webkit-details-marker { display: none; }
details > summary::before { content: "\\25B6"; display: inline-block; transition: transform 0.1s; }
details[open] > summary::before { transform: rotate(90deg); }
details > summary .name { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
details > summary .time { color: #57606a; font-weight: 400; font-size: 0.85rem; }
pre {
  margin: 0; padding: 0.75rem 1rem; overflow-x: auto; background: #f6f8fa;
  border-top: 1px solid #d0d7de; font-size: 0.82rem; line-height: 1.45;
  white-space: pre-wrap; word-break: break-word;
}
.empty { color: #57606a; padding: 0.75rem 1rem; border-top: 1px solid #d0d7de; margin: 0; }
"""


def clip(text: str, max_bytes: int) -> str:
    """Trim to max_bytes, keeping the tail, which is where a failure reports."""
    encoded = text.encode("utf-8", errors="replace")
    if len(encoded) <= max_bytes:
        return text
    kept = encoded[-max_bytes:].decode("utf-8", errors="replace")
    dropped = len(encoded) - max_bytes
    return (
        f"[{dropped} earlier byte(s) omitted by ut_console.py; "
        f"the tail is kept because that is where failures report]\n\n" + kept
    )


def render_page(bucket: Bucket, *, back_href: str, back_label: str, max_bytes: int) -> str:
    counts = bucket.counts
    blocks = []
    for case in bucket.cases:
        output = clip(case.output, max_bytes)
        body = (
            f"<pre>{html.escape(output)}</pre>"
            if output.strip()
            else '<p class="empty">This test produced no console output.</p>'
        )
        # Failures start expanded: that is what a reader following this link wants.
        open_attr = " open" if case.status == STATUS_FAILED else ""
        blocks.append(
            f"<details{open_attr}><summary>"
            f'<span class="pill {case.status}">{case.status}</span>'
            f'<span class="name">{html.escape(case.name)}</span>'
            f'<span class="time">{case.duration:.2f}s</span>'
            f"</summary>{body}</details>"
        )
    if not blocks:
        blocks.append('<p class="empty">No unit tests ran for this module.</p>')

    tally = f"{counts[STATUS_PASSED]} passed"
    if counts[STATUS_FAILED]:
        tally += f", {counts[STATUS_FAILED]} failed"
    if counts[STATUS_SKIPPED]:
        tally += f", {counts[STATUS_SKIPPED]} skipped"

    return (
        "<!DOCTYPE html>\n"
        '<html lang="en"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width, initial-scale=1">'
        f"<title>Unit tests &mdash; {html.escape(bucket.label)}</title>"
        f"<style>{CSS}</style></head><body>"
        f"<h1>Unit-test console output &mdash; <code>{html.escape(bucket.label)}</code></h1>"
        f'<div class="meta"><span class="pill {bucket.status}">{bucket.status}</span> '
        f"{html.escape(tally)} &middot; "
        f"{sum(c.duration for c in bucket.cases):.2f}s &middot; "
        f'<a href="{html.escape(back_href)}">{html.escape(back_label)}</a> &middot; '
        f'<a href="console.txt">plain text</a></div>'
        f"{''.join(blocks)}"
        "</body></html>\n"
    )


def render_text(bucket: Bucket, max_bytes: int) -> str:
    parts = [f"# unit-test console output: {bucket.label}", ""]
    for case in bucket.cases:
        parts += [
            "=" * 78,
            f"{case.status.upper()}  {case.name}  ({case.duration:.2f}s)",
            "=" * 78,
            clip(case.output, max_bytes).rstrip(),
            "",
        ]
    return "\n".join(parts) + "\n"


def write_bucket(
    directory: Path, bucket: Bucket, *, back_href: str, back_label: str, max_bytes: int
) -> None:
    directory.mkdir(parents=True, exist_ok=True)
    (directory / "index.html").write_text(
        render_page(bucket, back_href=back_href, back_label=back_label, max_bytes=max_bytes),
        encoding="utf-8",
    )
    (directory / "console.txt").write_text(render_text(bucket, max_bytes), encoding="utf-8")
    (directory / "summary.json").write_text(
        json.dumps(bucket.to_summary(), indent=2) + "\n", encoding="utf-8"
    )


# --------------------------------------------------------------------------- #
# Subcommands                                                                 #
# --------------------------------------------------------------------------- #


def cmd_split(args) -> int:
    dest = args.dest.resolve()
    if not args.junit.is_file():
        # Not fatal: the site renders the column as "no log" rather than failing a
        # publish because a test report went missing.
        print(f"::warning::ut_console: no JUnit report at {args.junit}; no pages written")
        return 0
    try:
        cases = parse_junit(args.junit)
    except ET.ParseError as exc:
        print(f"::warning::ut_console: {args.junit} is not parseable XML: {exc}")
        return 0

    records = [
        json.loads(line)
        for line in args.modules_jsonl.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    by_target = {
        target_name(r["path"], args.module_prefix, args.target_suffix): r["path"]
        for r in records
    }

    buckets: dict[str, Bucket] = {}
    unmatched: list[str] = []
    for case in cases:
        module = by_target.get(case.name)
        if module is None:
            unmatched.append(case.name)
            continue
        buckets.setdefault(module, Bucket(label=module)).cases.append(case)

    for module, bucket in buckets.items():
        depth = len([p for p in module.split("/") if p]) + 1
        write_bucket(
            dest / module / "tests",
            bucket,
            back_href="../" * depth + "index.html",
            back_label="← back to the module list",
            max_bytes=args.max_bytes,
        )

    # The whole-run page keeps every test, including any that did not map to a
    # module, so nothing in the report is silently dropped from the site.
    whole = Bucket(label=args.label, cases=cases)
    write_bucket(
        dest / "tests",
        whole,
        back_href="../index.html",
        back_label="← back to the module list",
        max_bytes=args.max_bytes,
    )

    if unmatched:
        print(
            f"::warning::ut_console: {len(unmatched)} test(s) did not map to a discovered "
            f"module and appear only on the whole-run page: {', '.join(sorted(unmatched)[:10])}"
        )
    print(
        f"ut_console: {len(cases)} test(s) across {len(buckets)} module(s) -> {dest}",
        file=sys.stderr,
    )
    return 0


def cmd_selftest(_args) -> int:
    """Offline self-test, run by format-check alongside the other shared scripts."""
    import tempfile

    failures: list[str] = []

    def check(label: str, got, want) -> None:
        if got != want:
            failures.append(f"{label}: expected {want!r}, got {got!r}")

    check(
        "target name",
        target_name("Drv/GpioDriver", "fprime-samd", "_ut_exe"),
        "fprime-samd_Drv_GpioDriver_ut_exe",
    )
    check("target name top level", target_name("Os", "fprime-samd", "_ut_exe"), "fprime-samd_Os_ut_exe")
    check("target name no prefix", target_name("Os", "", "_ut_exe"), "Os_ut_exe")

    # clip() keeps the tail, because that is where a failing assertion reports.
    clipped = clip("A" * 100 + "TAIL", 10)
    check("clip keeps tail", clipped.endswith("ATAIL"), True)
    check("clip notes omission", "omitted" in clipped, True)
    check("clip leaves short text alone", clip("short", 100), "short")

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        junit = root / "ut.xml"
        junit.write_text(
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<testsuite name="ctest" tests="3">\n'
            '  <testcase name="fprime-samd_Drv_GpioDriver_ut_exe" status="run" time="1.5">\n'
            "    <system-out>[  PASSED  ] 4 tests. &lt;tag&gt; &amp; more</system-out>\n"
            "  </testcase>\n"
            '  <testcase name="fprime-samd_Svc_Framer_ut_exe" status="fail" time="0.5">\n'
            '    <failure message="Assertion blew up">extra detail</failure>\n'
            "    <system-out>running framer</system-out>\n"
            "  </testcase>\n"
            '  <testcase name="harness_Something_ut_exe" status="run" time="0.25">\n'
            "    <system-out>not one of ours</system-out>\n"
            "  </testcase>\n"
            "</testsuite>\n",
            encoding="utf-8",
        )
        modules = root / "modules.jsonl"
        modules.write_text(
            json.dumps({"path": "Drv/GpioDriver", "has_ut": True}) + "\n"
            + json.dumps({"path": "Svc/Framer", "has_ut": True}) + "\n"
            + json.dumps({"path": "Os", "has_ut": False}) + "\n",
            encoding="utf-8",
        )
        dest = root / "work"
        check(
            "split",
            main(["split", "--junit", str(junit), "--modules-jsonl", str(modules),
                  "--dest", str(dest)]),
            0,
        )

        passing = json.loads((dest / "Drv/GpioDriver/tests/summary.json").read_text())
        check("passing status", passing["status"], "passed")
        check("passing count", passing["tests"], 1)
        check("passing duration", passing["duration"], 1.5)
        # The keys coverage_site.py reads off this file to render the Unit tests
        # column. Renaming one here silently turns that column into "no log".
        for key in ("status", "tests", "passed", "failed", "skipped"):
            if key not in passing:
                failures.append(f"summary.json is missing the {key!r} key coverage_site.py reads")

        failing = json.loads((dest / "Svc/Framer/tests/summary.json").read_text())
        check("failing status", failing["status"], "failed")
        check("failing count", failing["failed"], 1)

        # A module with no unit tests must not get a page, or the site would link to
        # an empty log instead of showing an honest dash.
        check("no page without tests", (dest / "Os/tests").exists(), False)

        whole = json.loads((dest / "tests/summary.json").read_text())
        check("whole-run keeps unmatched tests", whole["tests"], 3)

        page = (dest / "Drv/GpioDriver/tests/index.html").read_text(encoding="utf-8")
        check("console output is present", "[  PASSED  ] 4 tests." in page, True)
        # Test output is untrusted text: markup in it must render as text, not as
        # markup. The XML above carries the entities, so the parser hands us
        # "<tag> & more" and the page must escape it again.
        check("console output is escaped", "&lt;tag&gt; &amp; more" in page, True)
        check("console output is not injected", "<tag>" in page, False)
        if not (dest / "Drv/GpioDriver/tests/console.txt").is_file():
            failures.append("plain-text log missing")

        failing_page = (dest / "Svc/Framer/tests/index.html").read_text(encoding="utf-8")
        check("failure reason surfaced", "Assertion blew up" in failing_page, True)
        check("failure starts expanded", "<details open>" in failing_page, True)

        # ctest's bare "Failed" verdict on an ordinary non-zero exit adds nothing to a
        # log that already ends in the real assertion, and must not be appended.
        bare = root / "bare.xml"
        bare.write_text(
            '<testsuite><testcase name="fprime-samd_Drv_GpioDriver_ut_exe" status="fail" time="1">'
            '<failure message="Failed"></failure>'
            "<system-out>[  FAILED  ] the real assertion</system-out></testcase></testsuite>",
            encoding="utf-8",
        )
        bare_dest = root / "bare-work"
        main(["split", "--junit", str(bare), "--modules-jsonl", str(modules), "--dest", str(bare_dest)])
        bare_page = (bare_dest / "Drv/GpioDriver/tests/index.html").read_text(encoding="utf-8")
        check("bare verdict dropped", "ctest reported" in bare_page, False)
        check("real assertion kept", "[  FAILED  ] the real assertion" in bare_page, True)

        # A timeout is the opposite case: ctest knows it and the output does not.
        timeout = root / "timeout.xml"
        timeout.write_text(
            '<testsuite><testcase name="fprime-samd_Drv_GpioDriver_ut_exe" status="fail" time="9">'
            '<failure message="Timeout"></failure><system-out>hung</system-out></testcase></testsuite>',
            encoding="utf-8",
        )
        timeout_dest = root / "timeout-work"
        main(["split", "--junit", str(timeout), "--modules-jsonl", str(modules),
              "--dest", str(timeout_dest)])
        check(
            "timeout surfaced",
            "ctest reported: Timeout"
            in (timeout_dest / "Drv/GpioDriver/tests/index.html").read_text(encoding="utf-8"),
            True,
        )

        # A missing report is a warning, not a publish failure.
        check(
            "missing report tolerated",
            main(["split", "--junit", str(root / "nope.xml"),
                  "--modules-jsonl", str(modules), "--dest", str(dest)]),
            0,
        )

    for failure in failures:
        print(f"[FAIL] {failure}", file=sys.stderr)
    print(f"selftest: {'FAILED' if failures else 'passed'} ({len(failures)} failure(s))")
    return 1 if failures else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="command", required=True)

    split = sub.add_parser("split", help="write per-module console-log pages from a JUnit report")
    split.add_argument("--junit", type=Path, required=True, help="ctest --output-junit report")
    split.add_argument("--modules-jsonl", type=Path, required=True)
    split.add_argument("--dest", type=Path, required=True, help="modules root in the work tree")
    split.add_argument(
        "--module-prefix",
        default="fprime-samd",
        help="F Prime module-name prefix for this library (default: fprime-samd)",
    )
    split.add_argument("--target-suffix", default="_ut_exe")
    split.add_argument("--max-bytes", type=int, default=DEFAULT_MAX_BYTES)
    split.add_argument("--label", default="whole run", help="title for the whole-run page")
    split.set_defaults(func=cmd_split)

    selftest = sub.add_parser("selftest", help="run the offline self-test")
    selftest.set_defaults(func=cmd_selftest)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())

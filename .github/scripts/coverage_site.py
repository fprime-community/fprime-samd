#!/usr/bin/env python3
"""Build the coverage site that GitHub Pages serves from the coverage/main branch.

This stands in for nasa/fprime-actions' mirror.py + catalog.py. Those render a
five-column F Prime component-quality checklist -- UT coverage, integration
coverage, CodeQL, checklist checks, SDD -- with no way to turn a column off, so
publishing only coverage from here would leave three permanently empty columns, a
"no data" banner, and an overall badge stuck on bronze for every module (a missing
SDD grades bronze and always participates in the roll-up). fprime-samd publishes
coverage and nothing else, so it renders coverage and nothing else.

What is deliberately *not* re-implemented: the on-disk layout. Paths here match
upstream's exactly, because nasa/fprime-actions/coverage-check compares pull
requests against this branch and reads

    <root>/coverage/summary.json            library-wide
    <root>/<module>/coverage/summary.json   per module

Changing either path breaks the pull-request comment silently -- every module would
read as "new".

Two subcommands:

  publish   Copy a working tree's gcovr output into a destination on the branch and
            render index.html + catalog.json there. The destination is the branch
            root for a branch build, or releases/<tag> for a tag build.

  reindex   Re-render the branch root's index.html from the catalog.json already
            there. Used after a release publish so the new release shows up in the
            root page's release list without re-measuring main.
"""

from __future__ import annotations

import argparse
import datetime as dt
import html
import json
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

SCHEMA = 1

#: Tier cut-offs, overridable per repository via .github/module-checklist.yml.
#: Same keys and defaults as upstream so the file means one thing, not two.
DEFAULT_TIERS = {"platinum": 95.0, "gold": 90.0, "silver": 80.0}

TIER_ORDER = ("platinum", "gold", "silver", "bronze")

TIER_COLOURS = {
    "platinum": ("#eaeef2", "#57606a", "#afb8c1"),
    "gold": ("#fff8c5", "#7d4e00", "#eac54f"),
    "silver": ("#f6f8fa", "#57606a", "#d0d7de"),
    "bronze": ("#ffebe9", "#a40e26", "#ff8182"),
}

#: Unit-test result pills. Deliberately not the tier palette: a test run passes or
#: fails, it is not graded on a four-point scale.
STATUS_COLOURS = {
    "passed": ("#dafbe1", "#0a5122", "#4ac26b"),
    "failed": ("#ffebe9", "#a40e26", "#ff8182"),
    "skipped": ("#f6f8fa", "#57606a", "#d0d7de"),
}

#: Subdirectory holding the console-log pages ut_console.py writes, both per module
#: and at the modules root.
TESTS_SUBDIR = "tests"


# --------------------------------------------------------------------------- #
# gcovr --json-summary                                                        #
# --------------------------------------------------------------------------- #


@dataclass(frozen=True)
class Totals:
    covered: int
    total: int

    @property
    def percent(self) -> float:
        # total == 0 is normal: a module whose only code is headers has no
        # executable lines. Reported as 0% and treated as "no coverage".
        return round(100.0 * self.covered / self.total, 2) if self.total > 0 else 0.0


@dataclass(frozen=True)
class Summary:
    line: Totals
    function: Totals
    branch: Totals

    @property
    def measurable(self) -> bool:
        return self.line.total > 0

    @classmethod
    def from_gcovr(cls, doc: dict) -> "Summary":
        def totals(prefix: str) -> Totals:
            return Totals(
                covered=int(doc.get(f"{prefix}_covered", 0) or 0),
                total=int(doc.get(f"{prefix}_total", 0) or 0),
            )

        return cls(line=totals("line"), function=totals("function"), branch=totals("branch"))

    def to_entry(self) -> dict:
        return {
            "line_pct": self.line.percent,
            "line_covered": self.line.covered,
            "line_total": self.line.total,
            "function_pct": self.function.percent,
            "function_covered": self.function.covered,
            "function_total": self.function.total,
            "branch_pct": self.branch.percent,
            "branch_covered": self.branch.covered,
            "branch_total": self.branch.total,
        }

    @classmethod
    def from_entry(cls, entry: dict) -> "Summary":
        return cls(
            line=Totals(entry.get("line_covered", 0), entry.get("line_total", 0)),
            function=Totals(entry.get("function_covered", 0), entry.get("function_total", 0)),
            branch=Totals(entry.get("branch_covered", 0), entry.get("branch_total", 0)),
        )


@dataclass(frozen=True)
class Module:
    """One discovered module and everything published about it."""

    path: str
    has_ut: bool
    coverage: Optional[Summary]
    tests: Optional[dict]

    @property
    def group(self) -> str:
        return self.path.split("/", 1)[0]

    def sort_key(self) -> str:
        return self.path


def load_summary(path: Path) -> Optional[Summary]:
    """Read a gcovr --json-summary file, or None when absent or unreadable."""
    try:
        return Summary.from_gcovr(json.loads(path.read_text(encoding="utf-8")))
    except (OSError, ValueError):
        return None


# --------------------------------------------------------------------------- #
# Configuration and tiers                                                     #
# --------------------------------------------------------------------------- #


def load_tiers(path: Optional[Path]) -> dict:
    """Read coverage.tiers from the checklist config; defaults on any problem.

    PyYAML is preinstalled on GitHub-hosted runners; JSON is accepted as a
    fallback so the config still loads in a bare environment.
    """
    tiers = dict(DEFAULT_TIERS)
    if path is None or not path.is_file():
        return tiers
    try:
        text = path.read_text(encoding="utf-8")
        try:
            import yaml

            doc = yaml.safe_load(text)
        except ImportError:
            doc = json.loads(text)
    except Exception as exc:  # noqa: BLE001 -- any failure means defaults
        print(f"config: {path}: {exc}; using defaults", file=sys.stderr)
        return tiers
    configured = ((doc or {}).get("coverage") or {}).get("tiers") or {}
    for key in tiers:
        if key in configured:
            try:
                tiers[key] = float(configured[key])
            except (TypeError, ValueError):
                print(f"config: coverage.tiers.{key} is not a number; ignoring", file=sys.stderr)
    return tiers


def tier_of(summary: Optional[Summary], tiers: dict) -> str:
    """Badge tier from line coverage. No measurable coverage is bronze."""
    if summary is None or not summary.measurable:
        return "bronze"
    pct = summary.line.percent
    if pct >= tiers["platinum"]:
        return "platinum"
    if pct >= tiers["gold"]:
        return "gold"
    if pct >= tiers["silver"]:
        return "silver"
    return "bronze"


def worst_tier(candidates) -> str:
    ranked = [t for t in candidates if t in TIER_ORDER]
    return TIER_ORDER[max(TIER_ORDER.index(t) for t in ranked)] if ranked else "bronze"


# --------------------------------------------------------------------------- #
# Mirroring                                                                   #
# --------------------------------------------------------------------------- #


def clean_dir(path: Path) -> None:
    """Replace path with an empty directory, so a re-run leaves nothing stale."""
    if path.exists():
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        else:
            path.unlink()
    path.mkdir(parents=True, exist_ok=True)


def copy_reports(src: Path, dst: Path, *, rename_index: bool) -> None:
    """Copy gcovr's output files (not directories) from src to dst.

    A per-module report is renamed coverage.html -> index.html so the module's
    directory URL resolves. Its sibling coverage.*.html detail pages keep gcovr's
    names, because the index links to them by those names. The library-wide report
    is already called coverage-all.html and is left alone.
    """
    for entry in sorted(src.iterdir()):
        if entry.is_dir():
            continue
        name = "index.html" if rename_index and entry.name == "coverage.html" else entry.name
        shutil.copy2(entry, dst / name)


PLACEHOLDER_CSS = (
    "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; "
    "color: #1f2328; padding: 2rem; max-width: 40rem; } "
    "h1 { margin: 0 0 0.5rem 0; font-size: 1.1rem; } "
    ".meta { color: #57606a; font-size: 0.9rem; }"
)


def write_placeholder(path: Path, module: str, reason: str, back_href: str) -> None:
    path.write_text(
        "<!DOCTYPE html>\n"
        '<html lang="en"><head><meta charset="utf-8">'
        f"<title>No coverage &mdash; {html.escape(module)}</title>"
        f"<style>{PLACEHOLDER_CSS}</style></head><body>"
        f"<h1>No coverage recorded for <code>{html.escape(module)}</code></h1>"
        f"<p>{html.escape(reason)}</p>"
        f'<p class="meta"><a href="{html.escape(back_href)}">&larr; back to the module list</a></p>'
        "</body></html>\n",
        encoding="utf-8",
    )


def load_tests(path: Path) -> Optional[dict]:
    """Read a ut_console.py tests/summary.json, or None when absent or unreadable."""
    try:
        summary = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    return summary if isinstance(summary, dict) else None


def mirror(source: Path, dest: Path, records: list[dict]) -> tuple[dict, dict]:
    """Copy the library-wide and per-module reports into dest.

    Returns ({module: Summary or None}, {module: tests summary or None}). Only the
    paths written here are touched; anything else already on the branch (notably
    releases/) is left alone.
    """
    library_wide = source / "coverage"
    if library_wide.is_dir():
        target = dest / "coverage"
        clean_dir(target)
        copy_reports(library_wide, target, rename_index=False)

    # The whole-run console log, linked from the page banner.
    whole_run_tests = source / TESTS_SUBDIR
    if whole_run_tests.is_dir():
        target = dest / TESTS_SUBDIR
        clean_dir(target)
        copy_reports(whole_run_tests, target, rename_index=False)

    summaries: dict[str, Optional[Summary]] = {}
    tests: dict[str, Optional[dict]] = {}
    for record in records:
        module = record["path"]

        # Console logs are published whether or not coverage came out, so a module
        # whose test ran but produced no coverage data still has a readable log.
        src_tests = source / module / TESTS_SUBDIR
        tests[module] = load_tests(src_tests / "summary.json") if src_tests.is_dir() else None
        if tests[module] is not None:
            dst_tests = dest / module / TESTS_SUBDIR
            clean_dir(dst_tests)
            copy_reports(src_tests, dst_tests, rename_index=False)

        src = source / module / "coverage"
        dst = dest / module / "coverage"
        clean_dir(dst)

        summary = load_summary(src / "summary.json") if src.is_dir() else None
        if summary is not None and summary.measurable:
            copy_reports(src, dst, rename_index=True)
            summaries[module] = summary
            continue

        if not record.get("has_ut"):
            reason = (
                "This module is registered as an F Prime module but declares no unit "
                "tests (no register_fprime_ut() call), so there is nothing to measure."
            )
        else:
            reason = (
                "This module declares unit tests, but this run produced no coverage "
                "data for it (gcovr reported no measurable lines)."
            )
        # ../.. climbs out of <module>/coverage; module itself adds its own depth.
        depth = len([p for p in module.split("/") if p]) + 1
        write_placeholder(dst / "index.html", module, reason, "../" * depth + "index.html")
        summaries[module] = None
    return summaries, tests


# --------------------------------------------------------------------------- #
# Releases                                                                    #
# --------------------------------------------------------------------------- #


@dataclass(frozen=True)
class Release:
    tag: str
    commit: str
    generated_at: str
    overall: Optional[Summary]
    tier: str


def _version_key(tag: str):
    """Sort key that orders v1.10.0 after v1.9.0, with a stable string fallback."""
    numbers = tuple(int(n) for n in re.findall(r"\d+", tag))
    return (numbers, tag)


def find_releases(pages_root: Path, tiers: dict) -> list[Release]:
    """Every releases/<tag>/catalog.json on the branch, newest version first."""
    releases_dir = pages_root / "releases"
    if not releases_dir.is_dir():
        return []
    found: list[Release] = []
    for child in sorted(releases_dir.iterdir()):
        catalog_path = child / "catalog.json"
        if not child.is_dir() or not catalog_path.is_file():
            continue
        try:
            catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            print(f"releases: skipping unreadable {catalog_path}", file=sys.stderr)
            continue
        overall_entry = catalog.get("overall")
        overall = Summary.from_entry(overall_entry) if overall_entry else None
        found.append(
            Release(
                tag=catalog.get("ref") or child.name,
                commit=catalog.get("commit", ""),
                generated_at=catalog.get("generated_at", ""),
                overall=overall,
                tier=tier_of(overall, tiers),
            )
        )
    return sorted(found, key=lambda r: _version_key(r.tag), reverse=True)


RELEASES_INDEX_CSS = (
    "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; "
    "color: #1f2328; padding: 1.5rem; max-width: 50rem; } "
    "h1 { font-size: 1.3rem; margin: 0 0 1rem 0; } "
    "ul { list-style: none; padding: 0; } "
    "li { padding: 0.45rem 0; border-top: 1px solid #eaeef2; } "
    "a { color: #0969da; text-decoration: none; } a:hover { text-decoration: underline; } "
    ".meta { color: #57606a; font-size: 0.85rem; }"
)


def write_releases_index(pages_root: Path, releases: list[Release]) -> None:
    """A browsable page at /releases/, so that URL is not a 404."""
    if not releases:
        return
    items = "".join(
        f"<li><a href=\"{html.escape(r.tag)}/index.html\"><strong>{html.escape(r.tag)}</strong></a> "
        + (
            f"&mdash; {r.overall.line.percent:.2f}% line coverage "
            if r.overall and r.overall.measurable
            else "&mdash; no coverage data "
        )
        + f'<span class="meta">{html.escape(r.commit[:12])}'
        + (f" &middot; {html.escape(r.generated_at)}" if r.generated_at else "")
        + "</span></li>"
        for r in releases
    )
    (pages_root / "releases" / "index.html").write_text(
        "<!DOCTYPE html>\n"
        '<html lang="en"><head><meta charset="utf-8">'
        "<title>fprime-samd coverage by release</title>"
        f"<style>{RELEASES_INDEX_CSS}</style></head><body>"
        "<h1>fprime-samd coverage by release</h1>"
        '<p class="meta">Each entry is frozen at the tag it was built from and is a '
        "permanent link.</p>"
        f"<ul>{items}</ul>"
        '<p><a href="../index.html">&larr; current coverage</a></p>'
        "</body></html>\n",
        encoding="utf-8",
    )


# --------------------------------------------------------------------------- #
# The module list page                                                        #
# --------------------------------------------------------------------------- #

CSS = """* { box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
  margin: 0; padding: 1.5rem; color: #1f2328; background: #fff;
}
h1 { margin: 0 0 0.25rem 0; font-size: 1.4rem; }
.meta { color: #57606a; font-size: 0.9rem; margin-bottom: 1rem; }
.overall {
  display: flex; gap: 1rem; align-items: baseline; flex-wrap: wrap;
  padding: 0.75rem 1rem; background: #f6f8fa; border: 1px solid #d0d7de;
  border-radius: 6px; margin-bottom: 1rem;
}
a { color: #0969da; text-decoration: none; }
a:hover { text-decoration: underline; }
.section-title { font-weight: 600; margin: 1.25rem 0 0.5rem 0; }
details { border: 1px solid #d0d7de; border-radius: 6px; margin-bottom: 0.5rem; }
details > summary {
  cursor: pointer; padding: 0.5rem 0.75rem; font-weight: 600;
  display: grid; grid-template-columns: 1fr 14rem 14rem; gap: 0.5rem;
  align-items: baseline; list-style: none;
}
details > summary::-webkit-details-marker { display: none; }
details > summary::before { content: "\\25B6"; display: inline-block; margin-right: 0.4rem; transition: transform 0.1s; }
details[open] > summary::before { transform: rotate(90deg); }
table { width: 100%; border-collapse: collapse; }
table th, table td { padding: 0.35rem 0.75rem; text-align: left; }
table th { font-weight: 500; color: #57606a; font-size: 0.85rem; border-bottom: 1px solid #eaeef2; }
table td.cell, table th.cell { width: 14rem; white-space: nowrap; }
table tr.row { border-top: 1px solid #eaeef2; }
table tr.row:hover { background: #f6f8fa; }
.badge {
  display: inline-block; padding: 0.05rem 0.55rem; border-radius: 2em;
  font-size: 0.78rem; font-weight: 600; border: 1px solid transparent;
  vertical-align: baseline;
}
.no-cov { color: #57606a; }
.releases { margin: 0; padding: 0; list-style: none; }
.releases li { display: inline-block; margin: 0 0.75rem 0.35rem 0; }
"""


def badge(tier: str) -> str:
    background, colour, border = TIER_COLOURS.get(tier, TIER_COLOURS["bronze"])
    return (
        f'<span class="badge" style="background:{background};color:{colour};'
        f'border-color:{border}">{tier.capitalize()}</span>'
    )


def status_pill(status: str) -> str:
    background, colour, border = STATUS_COLOURS.get(status, STATUS_COLOURS["skipped"])
    return (
        f'<span class="badge" style="background:{background};color:{colour};'
        f'border-color:{border}">{html.escape(status)}</span>'
    )


def tests_cell(tests: Optional[dict], module: str, has_ut: bool) -> str:
    """The Unit tests column: result pill linking to that module's console output."""
    if tests is None:
        # No log to link to. "no UT" is the expected case; "no log" means a module
        # declares a unit test but nothing in the ctest report mapped to it, which is
        # worth showing rather than hiding behind an identical dash.
        return f'<span class="no-cov">{"no UT" if not has_ut else "no log"}</span>'
    # Deliberately not "1 of 1 passed": a module registers one ctest test per unit-test
    # executable, and that one test typically wraps dozens of gtest cases. Counting
    # ctest tests here would read as a count of test cases and understate the run. The
    # count is only shown when a module registers more than one executable, where it
    # actually distinguishes something.
    total = int(tests.get("tests", 0) or 0)
    label = "console output" if total <= 1 else f"console output ({total} executables)"
    href = f"{module}/{TESTS_SUBDIR}/index.html"
    return f'{status_pill(tests.get("status", "skipped"))} <a href="{html.escape(href)}">{label}</a>'


def coverage_cell(summary: Optional[Summary], href: str, tiers: dict, missing: str) -> str:
    if summary is None or not summary.measurable:
        return f'{badge("bronze")} <a href="{html.escape(href)}" class="no-cov">{missing}</a>'
    return (
        f"{badge(tier_of(summary, tiers))} "
        f'<a href="{html.escape(href)}">{summary.line.percent:.2f}% line</a>'
    )


def render_index(
    *,
    ref: str,
    ref_type: str,
    commit: str,
    generated_at: str,
    overall: Optional[Summary],
    modules: list[Module],
    tiers: dict,
    releases: list[Release],
    is_release_page: bool,
    whole_run_tests: Optional[dict] = None,
) -> str:
    """Render the module list."""
    if overall is not None and overall.measurable:
        parts = [
            f"<span><strong>Line coverage:</strong> {badge(tier_of(overall, tiers))} "
            f'<a href="coverage/coverage-all.html">{overall.line.percent:.2f}%</a> '
            f"({overall.function.percent:.2f}% function, {overall.branch.percent:.2f}% branch)"
            "</span>"
        ]
    else:
        parts = ['<span><strong>Line coverage:</strong> <span class="no-cov">no data</span></span>']
    if whole_run_tests is not None:
        passed = int(whole_run_tests.get("passed", 0) or 0)
        failed = int(whole_run_tests.get("failed", 0) or 0)
        total = int(whole_run_tests.get("tests", 0) or 0)
        # At the whole-library level the count is meaningful: one ctest test per
        # module unit-test executable, so this is "N of M module test binaries".
        tally = f"{failed} of {total} failed" if failed else f"{passed} of {total} passed"
        parts.append(
            f"<span><strong>Unit tests:</strong> "
            f'{status_pill(whole_run_tests.get("status", "skipped"))} '
            f'<a href="{TESTS_SUBDIR}/index.html">{tally}</a></span>'
        )
    banner = "".join(parts)

    groups: dict[str, list[Module]] = {}
    for module in modules:
        groups.setdefault(module.path.split("/", 1)[0], []).append(module)

    blocks = []
    for name in sorted(groups):
        members = groups[name]
        measured = [m.coverage for m in members if m.coverage is not None and m.coverage.measurable]
        rollup = Summary(
            line=Totals(sum(s.line.covered for s in measured), sum(s.line.total for s in measured)),
            function=Totals(
                sum(s.function.covered for s in measured), sum(s.function.total for s in measured)
            ),
            branch=Totals(
                sum(s.branch.covered for s in measured), sum(s.branch.total for s in measured)
            ),
        )
        if rollup.measurable:
            group_cell = f"{badge(tier_of(rollup, tiers))} {rollup.line.percent:.2f}% line"
        else:
            group_cell = f'{badge("bronze")} <span class="no-cov">&mdash;</span>'

        # The group's test cell is the worst result across its modules: one failing
        # test in a group must not be hidden behind a collapsed green summary.
        statuses = [m.tests["status"] for m in members if m.tests is not None]
        if not statuses:
            group_tests = '<span class="no-cov">&mdash;</span>'
        elif "failed" in statuses:
            group_tests = status_pill("failed")
        elif all(s == "skipped" for s in statuses):
            group_tests = status_pill("skipped")
        else:
            group_tests = status_pill("passed")

        rows = "".join(
            f'<tr class="row"><td>{html.escape(module.path)}</td>'
            f'<td class="cell">'
            + coverage_cell(
                module.coverage,
                f"{module.path}/coverage/index.html",
                tiers,
                "no UT" if not module.has_ut else "no coverage",
            )
            + '</td><td class="cell">'
            + tests_cell(module.tests, module.path, module.has_ut)
            + "</td></tr>"
            for module in sorted(members, key=Module.sort_key)
        )
        blocks.append(
            f"<details open><summary><span>{html.escape(name)}/</span>"
            f"<span>{group_cell}</span><span>{group_tests}</span></summary>"
            '<table><thead><tr><th>Module</th><th class="cell">Coverage</th>'
            '<th class="cell">Unit tests</th></tr></thead>'
            f"<tbody>{rows}</tbody></table></details>"
        )

    if is_release_page:
        nav = (
            '<p><a href="../../index.html">&larr; current coverage</a> &middot; '
            '<a href="../index.html">all releases</a></p>'
        )
    elif releases:
        items = "".join(
            f'<li>{badge(r.tier)} <a href="releases/{html.escape(r.tag)}/index.html">'
            f"{html.escape(r.tag)}</a></li>"
            for r in releases
        )
        nav = (
            '<div class="section-title">Releases</div>'
            f'<ul class="releases">{items}</ul>'
            '<p class="meta"><a href="releases/index.html">All releases</a> &mdash; each tag\'s '
            "report is frozen at the commit it was built from.</p>"
        )
    else:
        nav = ""

    return (
        "<!DOCTYPE html>\n"
        '<html lang="en"><head><meta charset="utf-8">'
        '<meta name="viewport" content="width=device-width, initial-scale=1">'
        f"<title>fprime-samd coverage &mdash; {html.escape(ref)}</title>"
        f"<style>{CSS}</style></head><body>"
        "<h1>fprime-samd unit-test coverage</h1>"
        f'<div class="meta">{html.escape(ref_type)} <code>{html.escape(ref)}</code> '
        f"@ <code>{html.escape(commit[:12])}</code> &middot; generated {html.escape(generated_at)}"
        "</div>"
        f'<div class="overall">{banner}</div>'
        f"{nav}"
        '<div class="section-title">Modules</div>'
        f"{''.join(blocks)}"
        "</body></html>\n"
    )


# --------------------------------------------------------------------------- #
# catalog.json                                                                #
# --------------------------------------------------------------------------- #


def build_catalog(
    *,
    ref: str,
    ref_type: str,
    commit: str,
    generated_at: str,
    overall: Optional[Summary],
    modules: list[Module],
    tiers: dict,
    whole_run_tests: Optional[dict] = None,
) -> dict:
    module_entries = []
    for module in sorted(modules, key=Module.sort_key):
        entry = None
        if module.coverage is not None and module.coverage.measurable:
            entry = module.coverage.to_entry()
            entry["report"] = f"{module.path}/coverage/index.html"
        tests_entry = None
        if module.tests is not None:
            # Drop the per-case list: catalog.json is an index, and the cases are
            # already on the module's own page.
            tests_entry = {k: v for k, v in module.tests.items() if k != "cases"}
            tests_entry["report"] = f"{module.path}/{TESTS_SUBDIR}/index.html"
        module_entries.append(
            {
                "path": module.path,
                "has_ut": module.has_ut,
                "has_coverage": entry is not None,
                "coverage": entry,
                "tests": tests_entry,
                "tier": tier_of(module.coverage, tiers),
            }
        )
    overall_entry = None
    if overall is not None and overall.measurable:
        overall_entry = overall.to_entry()
        overall_entry["report"] = "coverage/coverage-all.html"
    return {
        "schema": SCHEMA,
        "generator": "fprime-samd/.github/scripts/coverage_site.py",
        "ref": ref,
        "ref_type": ref_type,
        "commit": commit,
        "generated_at": generated_at,
        "thresholds": tiers,
        "overall": overall_entry,
        "overall_tier": tier_of(overall, tiers) if overall_entry else None,
        "worst_module_tier": worst_tier(m["tier"] for m in module_entries),
        "tests": (
            {k: v for k, v in whole_run_tests.items() if k != "cases"}
            | {"report": f"{TESTS_SUBDIR}/index.html"}
            if whole_run_tests is not None
            else None
        ),
        "modules": module_entries,
    }


# --------------------------------------------------------------------------- #
# Subcommands                                                                 #
# --------------------------------------------------------------------------- #


def cmd_publish(args) -> int:
    source, dest = args.source.resolve(), args.dest.resolve()
    pages_root = (args.pages_root or args.dest).resolve()
    tiers = load_tiers(args.config)

    if not args.modules_jsonl.is_file():
        print(f"publish: missing module list {args.modules_jsonl}", file=sys.stderr)
        return 2
    records = [
        json.loads(line)
        for line in args.modules_jsonl.read_text(encoding="utf-8").splitlines()
        if line.strip()
    ]
    if not records:
        print("publish: module list is empty; refusing to publish an empty site", file=sys.stderr)
        return 2

    dest.mkdir(parents=True, exist_ok=True)
    summaries, tests = mirror(source, dest, records)
    modules = [
        Module(
            path=r["path"],
            has_ut=bool(r.get("has_ut")),
            coverage=summaries.get(r["path"]),
            tests=tests.get(r["path"]),
        )
        for r in records
    ]
    overall = load_summary(source / "coverage" / "summary.json")
    whole_run_tests = load_tests(source / TESTS_SUBDIR / "summary.json")
    generated_at = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    is_release_page = dest != pages_root
    catalog = build_catalog(
        ref=args.ref,
        ref_type=args.ref_type,
        commit=args.commit,
        generated_at=generated_at,
        overall=overall,
        modules=modules,
        tiers=tiers,
        whole_run_tests=whole_run_tests,
    )
    (dest / "catalog.json").write_text(json.dumps(catalog, indent=2) + "\n", encoding="utf-8")

    # A release page must not advertise a release list of its own: it is frozen, so
    # anything newer than it does not belong on it.
    releases = [] if is_release_page else find_releases(pages_root, tiers)
    (dest / "index.html").write_text(
        render_index(
            ref=args.ref,
            ref_type=args.ref_type,
            commit=args.commit,
            generated_at=generated_at,
            overall=overall,
            modules=modules,
            tiers=tiers,
            releases=releases,
            is_release_page=is_release_page,
            whole_run_tests=whole_run_tests,
        ),
        encoding="utf-8",
    )
    if is_release_page:
        write_releases_index(pages_root, find_releases(pages_root, tiers))

    measured = sum(1 for m in modules if m.coverage is not None and m.coverage.measurable)
    print(
        f"publish: {len(modules)} module(s), {measured} with coverage -> {dest}",
        file=sys.stderr,
    )
    return 0


def cmd_reindex(args) -> int:
    """Re-render the root page from the catalog already on the branch.

    Used after a release publish: the root's coverage data has not changed, but its
    release list has. Reading the catalog back rather than re-measuring is what keeps
    a tag build from having to rebuild main.
    """
    dest = args.dest.resolve()
    tiers = load_tiers(args.config)
    catalog_path = dest / "catalog.json"
    if not catalog_path.is_file():
        # First publish on this branch was a tag, so there is no root page yet. The
        # next branch publish writes one, including the release list.
        print(f"reindex: no {catalog_path}; nothing to re-render", file=sys.stderr)
        return 0
    try:
        catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        print(f"reindex: cannot read {catalog_path}: {exc}", file=sys.stderr)
        return 1

    modules = [
        Module(
            path=m["path"],
            has_ut=bool(m.get("has_ut")),
            coverage=Summary.from_entry(m["coverage"]) if m.get("coverage") else None,
            tests=m.get("tests"),
        )
        for m in catalog.get("modules", [])
    ]
    overall_entry = catalog.get("overall")
    (dest / "index.html").write_text(
        render_index(
            ref=catalog.get("ref", ""),
            ref_type=catalog.get("ref_type", "branch"),
            commit=catalog.get("commit", ""),
            generated_at=catalog.get("generated_at", ""),
            overall=Summary.from_entry(overall_entry) if overall_entry else None,
            modules=modules,
            tiers=tiers,
            releases=find_releases(dest, tiers),
            is_release_page=False,
            whole_run_tests=catalog.get("tests"),
        ),
        encoding="utf-8",
    )
    write_releases_index(dest, find_releases(dest, tiers))
    print(f"reindex: re-rendered {dest / 'index.html'}", file=sys.stderr)
    return 0


def cmd_selftest(_args) -> int:
    """Exercise the site build without a toolchain, a build cache, or a network.

    Runs in format-check alongside size_report.py's self-test. The layout assertions
    are the important ones: nasa/fprime-actions/coverage-check reads this branch, and
    if a path moves the pull-request comment reports every module as new rather than
    failing loudly.
    """
    import tempfile

    failures: list[str] = []

    def check(label: str, got, want) -> None:
        if got != want:
            failures.append(f"{label}: expected {want!r}, got {got!r}")

    tiers = {"platinum": 95.0, "gold": 90.0, "silver": 80.0}

    def summary(covered: int, total: int) -> Summary:
        return Summary(Totals(covered, total), Totals(covered, total), Totals(covered, total))

    # Tier boundaries are inclusive at the cut-off.
    check("tier 95%", tier_of(summary(95, 100), tiers), "platinum")
    check("tier 94.99%", tier_of(summary(9499, 10000), tiers), "gold")
    check("tier 90%", tier_of(summary(90, 100), tiers), "gold")
    check("tier 80%", tier_of(summary(80, 100), tiers), "silver")
    check("tier 79%", tier_of(summary(79, 100), tiers), "bronze")
    # A header-only module has no executable lines: not 100%, no coverage.
    check("tier no lines", tier_of(summary(0, 0), tiers), "bronze")
    check("tier absent", tier_of(None, tiers), "bronze")
    check("worst tier", worst_tier(["platinum", "silver", "gold"]), "silver")

    # Releases must sort by version, not lexically: v1.10.0 is newer than v1.9.0.
    ordered = sorted(["v1.0.0", "v1.10.0", "v1.9.0", "v2.0.0"], key=_version_key, reverse=True)
    check("version sort", ordered, ["v2.0.0", "v1.10.0", "v1.9.0", "v1.0.0"])

    check(
        "summary round-trip",
        Summary.from_entry(summary(7, 10).to_entry()).line.percent,
        70.0,
    )

    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        work, pages = root / "work", root / "pages"
        modules = [
            {"path": "Drv/GpioDriver", "has_ut": True},
            {"path": "Svc/Framer", "has_ut": True},
            {"path": "Svc/FatalHandler", "has_ut": False},
            {"path": "Os", "has_ut": True},
        ]
        jsonl = root / "modules.jsonl"
        jsonl.write_text("".join(json.dumps(m) + "\n" for m in modules), encoding="utf-8")

        def seed(covered: int, total: int, *, framer_fails: bool = False) -> None:
            """Lay out what measure-coverage leaves in the work tree."""
            shutil.rmtree(work, ignore_errors=True)
            # tests/summary.json as ut_console.py writes it. These five keys are the
            # contract between the two scripts; ut_console's own self-test pins the
            # producing side.
            for module in ("Drv/GpioDriver", "Svc/Framer"):
                failed = framer_fails and module == "Svc/Framer"
                logs = work / module / TESTS_SUBDIR
                logs.mkdir(parents=True)
                (logs / "summary.json").write_text(
                    json.dumps(
                        {
                            "status": "failed" if failed else "passed",
                            "tests": 1,
                            "passed": 0 if failed else 1,
                            "failed": 1 if failed else 0,
                            "skipped": 0,
                        }
                    )
                )
                (logs / "index.html").write_text("<html>console</html>")
                (logs / "console.txt").write_text("console\n")
            whole = work / TESTS_SUBDIR
            whole.mkdir(parents=True)
            (whole / "summary.json").write_text(
                json.dumps(
                    {
                        "status": "failed" if framer_fails else "passed",
                        "tests": 2,
                        "passed": 1 if framer_fails else 2,
                        "failed": 1 if framer_fails else 0,
                        "skipped": 0,
                    }
                )
            )
            (whole / "index.html").write_text("<html>whole run</html>")

            for module in ("Drv/GpioDriver", "Svc/Framer", "Os"):
                directory = work / module / "coverage"
                directory.mkdir(parents=True)
                # to_entry() emits the same *_covered / *_total keys gcovr's
                # --json-summary does, which is what load_summary reads back.
                (directory / "summary.json").write_text(
                    json.dumps(summary(covered, total).to_entry())
                )
                (directory / "coverage.html").write_text("<html>module</html>")
            wide = work / "coverage"
            wide.mkdir(parents=True)
            (wide / "summary.json").write_text(
                json.dumps(summary(covered * 3, total * 3).to_entry())
            )
            (wide / "coverage-all.html").write_text("<html>library-wide</html>")

        common = ["--modules-jsonl", str(jsonl), "--commit", "0123456789abcdef"]
        seed(92, 100)
        check(
            "branch publish",
            main(["publish", "--source", str(work), "--dest", str(pages),
                  "--ref", "main", "--ref-type", "branch"] + common),
            0,
        )

        # The exact paths coverage-check's compare.py reads.
        for relative in ("coverage/summary.json", "Drv/GpioDriver/coverage/summary.json"):
            if not (pages / relative).is_file():
                failures.append(f"layout: missing {relative}")
        # coverage.html is renamed so the module's directory URL resolves.
        if not (pages / "Drv/GpioDriver/coverage/index.html").is_file():
            failures.append("layout: module coverage.html was not renamed to index.html")
        # A module without unit tests still gets a page, so its row is not a dead link.
        if not (pages / "Svc/FatalHandler/coverage/index.html").is_file():
            failures.append("layout: missing placeholder for a module with no UT")
        # Console logs are published per module and for the whole run.
        for relative in ("tests/index.html", "Drv/GpioDriver/tests/index.html",
                         "Drv/GpioDriver/tests/console.txt"):
            if not (pages / relative).is_file():
                failures.append(f"layout: missing {relative}")
        # A module with no unit tests must not get a log directory to link to.
        if (pages / "Svc/FatalHandler" / TESTS_SUBDIR).exists():
            failures.append("layout: published a console log for a module with no UT")

        branch_index = (pages / "index.html").read_text(encoding="utf-8")
        for expected in (
            "<th class=\"cell\">Unit tests</th>",
            'href="Drv/GpioDriver/tests/index.html"',
            "<strong>Unit tests:</strong>",
            'href="tests/index.html"',
        ):
            if expected not in branch_index:
                failures.append(f"render: root page is missing {expected!r}")
        # A module with no unit tests reads "no UT", not a broken link.
        if 'href="Svc/FatalHandler/tests/index.html"' in branch_index:
            failures.append("render: linked a console log that was never published")

        seed(60, 100)
        check(
            "release publish",
            main(["publish", "--source", str(work), "--dest", str(pages / "releases" / "v1.0.0"),
                  "--pages-root", str(pages), "--ref", "v1.0.0", "--ref-type", "tag"] + common),
            0,
        )
        check("reindex", main(["reindex", "--dest", str(pages)]), 0)

        index = (pages / "index.html").read_text(encoding="utf-8")
        for dropped in ("Int coverage", "CodeQL", "int-coverage", "SDD"):
            if dropped in index:
                failures.append(f"render: root page still mentions {dropped!r}")
        if 'href="releases/v1.0.0/index.html"' not in index:
            failures.append("render: root page does not link the published release")
        if not (pages / "releases" / "index.html").is_file():
            failures.append("render: no releases index page")

        release_index = (pages / "releases" / "v1.0.0" / "index.html").read_text(encoding="utf-8")
        if 'class="releases"' in release_index:
            failures.append("render: a frozen release page must not carry a release list")

        # A later branch publish must leave the release byte-for-byte alone.
        frozen = (pages / "releases" / "v1.0.0" / "catalog.json").read_bytes()
        seed(99, 100)
        main(["publish", "--source", str(work), "--dest", str(pages),
              "--ref", "main", "--ref-type", "branch"] + common)
        if (pages / "releases" / "v1.0.0" / "catalog.json").read_bytes() != frozen:
            failures.append("release: a branch publish rewrote a frozen release")

        catalog = json.loads((pages / "catalog.json").read_text(encoding="utf-8"))
        check("catalog modules", len(catalog["modules"]), 4)
        check("catalog overall tier", catalog["overall_tier"], "platinum")
        check("catalog whole-run tests", catalog["tests"]["status"], "passed")
        by_path = {m["path"]: m for m in catalog["modules"]}
        check("catalog no-UT module", by_path["Svc/FatalHandler"]["has_coverage"], False)
        check("catalog no-UT tier", by_path["Svc/FatalHandler"]["tier"], "bronze")
        check("catalog module tests", by_path["Drv/GpioDriver"]["tests"]["status"], "passed")
        check(
            "catalog module tests report",
            by_path["Drv/GpioDriver"]["tests"]["report"],
            "Drv/GpioDriver/tests/index.html",
        )
        check("catalog no-UT tests", by_path["Svc/FatalHandler"]["tests"], None)

        # A failing test must be visible on the collapsed group summary, not only
        # inside the expanded row.
        seed(99, 100, framer_fails=True)
        main(["publish", "--source", str(work), "--dest", str(pages),
              "--ref", "main", "--ref-type", "branch"] + common)
        failing_index = (pages / "index.html").read_text(encoding="utf-8")
        svc_summary = failing_index[failing_index.index("<span>Svc/</span>"):]
        svc_summary = svc_summary[: svc_summary.index("</summary>")]
        check("failed group summary", "failed" in svc_summary, True)
        drv_summary = failing_index[failing_index.index("<span>Drv/</span>"):]
        drv_summary = drv_summary[: drv_summary.index("</summary>")]
        check("passing group summary", "failed" in drv_summary, False)

    for failure in failures:
        print(f"[FAIL] {failure}", file=sys.stderr)
    print(f"selftest: {'FAILED' if failures else 'passed'} ({len(failures)} failure(s))")
    return 1 if failures else 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    publish = sub.add_parser("publish", help="mirror a working tree's gcovr output and render")
    publish.add_argument("--source", type=Path, required=True, help="modules root in the work tree")
    publish.add_argument("--dest", type=Path, required=True, help="where this report is published")
    publish.add_argument(
        "--pages-root",
        type=Path,
        default=None,
        help="branch root, when --dest is a releases/<tag> subdirectory of it",
    )
    publish.add_argument("--modules-jsonl", type=Path, required=True)
    publish.add_argument("--ref", required=True)
    publish.add_argument("--ref-type", default="branch", choices=("branch", "tag"))
    publish.add_argument("--commit", required=True)
    publish.add_argument("--config", type=Path, default=None)
    publish.set_defaults(func=cmd_publish)

    reindex = sub.add_parser("reindex", help="re-render the root page from its catalog.json")
    reindex.add_argument("--dest", type=Path, required=True, help="branch root")
    reindex.add_argument("--config", type=Path, default=None)
    reindex.set_defaults(func=cmd_reindex)

    selftest = sub.add_parser("selftest", help="run the offline self-test (used by format-check)")
    selftest.set_defaults(func=cmd_selftest)

    args = parser.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())

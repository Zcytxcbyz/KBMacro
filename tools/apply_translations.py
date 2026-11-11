#!/usr/bin/env python3
"""Apply line-based comment translations listed in a TSV file.

Each row is:  <relative path>\t<1-based line number>\t<full replacement line>

Blank lines and rows starting with '#' are ignored. A row is only applied when
the target line currently contains a CJK character, so a stale line number is
reported instead of silently overwriting unrelated code.

Usage:
    python tools/apply_translations.py --dry-run
    python tools/apply_translations.py
"""

from __future__ import annotations

import argparse
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TSV = ROOT / "tools" / "translations.tsv"


def has_cjk(text: str) -> bool:
    return any("\u4e00" <= char <= "\u9fff" for char in text)


def load_rows(path: Path) -> dict[str, list[tuple[int, str]]]:
    rows: dict[str, list[tuple[int, str]]] = defaultdict(list)
    for number, raw in enumerate(path.read_text(encoding="utf-8-sig").splitlines(), start=1):
        if not raw.strip() or raw.lstrip().startswith("#"):
            continue
        parts = raw.split("\t")
        if len(parts) != 3:
            raise SystemExit(f"{path.name}:{number}: expected 3 tab-separated columns, got {len(parts)}")
        relative, line_text, replacement = parts
        rows[relative.strip()].append((int(line_text), replacement))
    return rows


def apply_file(relative: str, edits: list[tuple[int, str]], dry_run: bool) -> tuple[int, list[str]]:
    path = ROOT / relative
    if not path.is_file():
        return 0, [f"{relative}: file not found"]

    text = path.read_text(encoding="utf-8")
    lines = text.split("\n")
    applied = 0
    problems: list[str] = []

    for number, replacement in sorted(edits):
        if not 1 <= number <= len(lines):
            problems.append(f"{relative}:{number}: out of range ({len(lines)} lines)")
            continue
        current = lines[number - 1]
        if not has_cjk(current):
            problems.append(f"{relative}:{number}: no CJK on line, skipped "
                            f"({current.strip()[:60]!r})")
            continue
        lines[number - 1] = replacement
        applied += 1

    if applied and not dry_run:
        path.write_text("\n".join(lines), encoding="utf-8", newline="")
    return applied, problems


def main() -> int:
    parser = argparse.ArgumentParser(description="Apply comment translations from a TSV file.")
    parser.add_argument("--file", default=str(DEFAULT_TSV), help="TSV file to apply")
    parser.add_argument("--dry-run", action="store_true", help="report without writing")
    args = parser.parse_args()

    tsv = Path(args.file)
    if not tsv.is_file():
        print(f"error: {tsv} not found", file=sys.stderr)
        return 2

    rows = load_rows(tsv)
    total = 0
    problems: list[str] = []
    for relative, edits in sorted(rows.items()):
        applied, issues = apply_file(relative, edits, args.dry_run)
        total += applied
        problems += issues
        print(f"{'would apply' if args.dry_run else 'applied'} {applied:3d}  {relative}")

    print(f"\ntotal: {total} line(s)")
    if problems:
        print(f"skipped {len(problems)}:")
        for issue in problems:
            print(f"  {issue}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

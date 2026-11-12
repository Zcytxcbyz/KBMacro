#!/usr/bin/env python3
"""Report which files still contain Chinese (CJK) characters.

KBMacro keeps every source comment in English and every user-facing string in
res/langs/*.rc. This script finds the files that break that rule, so a stray
Chinese comment does not slip through review.

Usage:
    python tools/check_chinese.py                # src/ tests/ res/ tools/ docs/
    python tools/check_chinese.py --lines        # also print the matching lines
    python tools/check_chinese.py --fail         # exit 1 when anything is found
    python tools/check_chinese.py --all          # walk the tree instead of git ls-files

    # CI-style check that only legitimately Chinese files are skipped:
    python tools/check_chinese.py -e "res/langs/*.rc" -e "tests/test_utf8.cpp" ^
                                  -e "docs/*" --fail

Notes:
    - By default only git-tracked files are scanned, so build/, .git/ and other
      ignored directories are skipped for free. --all falls back to a plain
      walk.
    - Files that are not valid UTF-8 or contain NUL bytes are skipped and
      listed separately, since Chinese hidden in a GBK-encoded file would
      otherwise look like "no hits".
"""

from __future__ import annotations

import argparse
import fnmatch
import re
import subprocess
import sys
from pathlib import Path

# CJK punctuation (3000-303F), extension A (3400-4DBF), unified ideographs
# (4E00-9FFF), compatibility ideographs (F900-FAFF) and fullwidth forms
# (FF00-FFEF).
CJK = re.compile("[\u3000-\u303f\u3400-\u4dbf\u4e00-\u9fff\uf900-\ufaff\uff00-\uffef]")

DEFAULT_ROOTS = ("src", "tests", "res", "tools", "docs")

SKIP_DIRS = {".git", ".vs", ".idea", "build", "out"}


def git_tracked(root: Path) -> list[Path] | None:
    """Git-tracked files under root, or None when git is unavailable."""
    try:
        completed = subprocess.run(
            ["git", "-C", str(root), "ls-files", "-z"],
            capture_output=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None

    names = [name for name in completed.stdout.decode("utf-8", "replace").split("\0") if name]
    return [root / name for name in names]


def walked(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if path.is_file() and not any(part in SKIP_DIRS for part in path.parts):
            files.append(path)
    return files


def scan(path: Path) -> tuple[list[tuple[int, str]], int] | str:
    """Return (lines_with_cjk, cjk_char_count) or a reason string when skipped."""
    try:
        raw = path.read_bytes()
    except OSError as exc:
        return f"unreadable ({exc})"

    if b"\x00" in raw[:8192]:
        return "binary"

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return "not UTF-8"

    hits: list[tuple[int, str]] = []
    chars = 0
    for number, line in enumerate(text.splitlines(), start=1):
        found = CJK.findall(line)
        if found:
            chars += len(found)
            hits.append((number, line.strip()))
    return hits, chars


def collect(root: Path, targets: list[Path], use_walk: bool) -> list[Path]:
    if use_walk:
        files: list[Path] = []
        for target in targets:
            if target.is_dir():
                files.extend(walked(target))
            elif target.is_file():
                files.append(target)
        return files

    tracked = git_tracked(root)
    if tracked is None:
        return collect(root, targets, use_walk=True)

    roots = [target.resolve() for target in targets]

    selected: list[Path] = []
    for path in tracked:
        resolved = path.resolve()
        if any(resolved == candidate or candidate in resolved.parents for candidate in roots):
            selected.append(path)
    return selected


def main() -> int:
    parser = argparse.ArgumentParser(description="Report files containing Chinese characters.")
    parser.add_argument(
        "paths",
        nargs="*",
        help=f"files or directories to scan (default: {' '.join(DEFAULT_ROOTS)})",
    )
    parser.add_argument(
        "-e",
        "--exclude",
        action="append",
        default=[],
        metavar="GLOB",
        help="skip files whose repo-relative path matches this glob (repeatable)",
    )
    parser.add_argument("--lines", action="store_true", help="print every matching line")
    parser.add_argument("--max-lines", type=int, default=20, help="lines printed per file (default 20)")
    parser.add_argument("--all", action="store_true", help="walk the tree instead of asking git")
    parser.add_argument("--fail", action="store_true", help="exit 1 when anything is found")
    args = parser.parse_args()

    root = Path(__file__).resolve().parent.parent
    targets = [Path(p) for p in args.paths] if args.paths else [root / name for name in DEFAULT_ROOTS]
    targets = [target for target in targets if target.exists()]
    if not targets:
        print("error: nothing to scan", file=sys.stderr)
        return 2

    rows: list[tuple[str, list[tuple[int, str]], int]] = []
    skipped: list[tuple[str, str]] = []

    for path in sorted(set(collect(root, targets, args.all))):
        try:
            relative = path.relative_to(root).as_posix()
        except ValueError:
            relative = path.as_posix()

        if any(fnmatch.fnmatch(relative, pattern) for pattern in args.exclude):
            continue

        result = scan(path)
        if isinstance(result, str):
            if result != "binary":
                skipped.append((relative, result))
            continue

        hits, chars = result
        if hits:
            rows.append((relative, hits, chars))

    for relative, reason in skipped:
        print(f"skipped  {relative}  ({reason})")

    if not rows:
        print("no Chinese found")
        return 0

    rows.sort(key=lambda row: (-len(row[1]), row[0]))
    total_lines = 0
    total_chars = 0
    for relative, hits, chars in rows:
        total_lines += len(hits)
        total_chars += chars
        print(f"{relative}  ({len(hits)} line(s), {chars} char(s))")
        if args.lines:
            for number, text in hits[: args.max_lines]:
                print(f"    {number:>4}: {text}")
            if len(hits) > args.max_lines:
                print(f"    ... {len(hits) - args.max_lines} more line(s)")

    print()
    print(f"{len(rows)} file(s), {total_lines} line(s), {total_chars} char(s) with Chinese")
    return 1 if args.fail else 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Align the string entries in KBMacro's .rc language files.

Every line of the form

    L(IDS_SOMETHING)        "text"

gets its string literal moved to the same column across all given files. Only
the whitespace between ")" and the opening quote is touched: indentation, ids,
text, comments, encoding, BOM and line endings stay byte-identical.

Usage:
    python tools/format_rc.py res/langs/zh-CN.rc res/langs/en-US.rc
    python tools/format_rc.py --check res/langs/zh-CN.rc res/langs/en-US.rc

Notes:
    - Pass every language file in one run so they share one column; running
      them separately can end up with different columns.
    - --check changes nothing and exits 1 when some file is not aligned yet,
      so it can be used as a cheap pre-commit guard.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# L(IDS_XXX) followed by a single-line string literal and nothing else.
ENTRY = re.compile(r'^(?P<indent>[ \t]*)L\((?P<id>[A-Za-z0-9_]+)\)[ \t]*"(?P<text>.*)"[ \t]*$')

# Minimum number of spaces between ")" and the opening quote.
MIN_GAP = 2


def read_text(path: Path) -> tuple[str, bool]:
    """Return (text without BOM, had_bom)."""
    raw = path.read_bytes()
    has_bom = raw.startswith(b"\xef\xbb\xbf")
    return raw.decode("utf-8-sig" if has_bom else "utf-8"), has_bom


def write_text(path: Path, text: str, has_bom: bool) -> None:
    data = text.encode("utf-8")
    if has_bom:
        data = b"\xef\xbb\xbf" + data
    path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description="Align .rc string entries.")
    parser.add_argument("files", nargs="+", type=Path, help=".rc files to process")
    parser.add_argument("--check", action="store_true", help="report without writing")
    args = parser.parse_args()

    for path in args.files:
        if not path.is_file():
            print(f"error: not a file: {path}", file=sys.stderr)
            return 2

    # Pass 1: widest "L(IDS_...)" token over all files, so every file shares
    # one column.
    widest = 0
    contents: dict[Path, tuple[str, bool]] = {}
    for path in args.files:
        text, has_bom = read_text(path)
        contents[path] = (text, has_bom)
        for line in text.splitlines():
            match = ENTRY.match(line)
            if match:
                widest = max(widest, len(f"L({match.group('id')})"))

    column = widest + MIN_GAP
    pending = 0

    for path in args.files:
        text, has_bom = contents[path]
        changed = 0
        out: list[str] = []
        for line in text.splitlines(keepends=True):
            body = line.rstrip("\r\n")
            ending = line[len(body):]
            match = ENTRY.match(body)
            if not match:
                out.append(line)
                continue
            token = f"L({match.group('id')})"
            gap = " " * (column - len(token))
            aligned = f"{match.group('indent')}{token}{gap}\"{match.group('text')}\""
            if aligned != body:
                changed += 1
            out.append(aligned + ending)

        pending += changed
        if args.check:
            print(f"{path}: {changed} line(s) need alignment")
        else:
            write_text(path, "".join(out), has_bom)
            print(f"{path}: {changed} line(s) aligned to column {column}")

    if args.check:
        return 1 if pending else 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

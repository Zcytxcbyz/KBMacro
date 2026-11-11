#!/usr/bin/env python3
"""Verify that a working-tree change touched comments only.

For every .hpp/.cpp under src/ and tests/, compare the committed version
(HEAD) with the working tree after removing comments and blank lines. Any
difference in the remaining code lines is a failure.

Also reports files that still contain CJK characters and files whose encoding
or line endings changed.

Usage:
    python tools/check_comments_only.py            # check whole tree
    python tools/check_comments_only.py --verbose  # list every file, not only failures
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = ("src", "tests")


def git_show(path: str) -> str | None:
    result = subprocess.run(["git", "show", f"HEAD:{path}"], cwd=ROOT,
                            capture_output=True)
    if result.returncode != 0:
        return None
    return result.stdout.decode("utf-8", errors="replace")


def strip_comments(text: str) -> list[str]:
    """Return the code-only lines that carry actual tokens."""
    out: list[str] = []
    current: list[str] = []
    i = 0
    length = len(text)

    def flush() -> None:
        line = "".join(current).strip()
        if line:
            out.append(line)
        current.clear()

    while i < length:
        char = text[i]
        nxt = text[i + 1] if i + 1 < length else ""

        if char == "\n":
            flush()
            i += 1
            continue

        # line comment
        if char == "/" and nxt == "/":
            while i < length and text[i] != "\n":
                i += 1
            continue

        # block comment
        if char == "/" and nxt == "*":
            i += 2
            while i < length and not (text[i] == "*" and i + 1 < length and text[i + 1] == "/"):
                if text[i] == "\n":
                    flush()
                i += 1
            i += 2
            continue

        # raw string literal R"(...)"
        if char == "R" and nxt == '"':
            start = i
            j = i + 2
            delimiter = ""
            while j < length and text[j] not in "(":
                delimiter += text[j]
                j += 1
            closing = ")" + delimiter + '"'
            end = text.find(closing, j)
            if end == -1:
                current.append(text[start:])
                break
            current.extend(text[start:end + len(closing)])
            i = end + len(closing)
            continue

        # C++14 digit separator (10'000): a quote that sits between two digits is
        # part of the number, not a character literal -- otherwise the scanner
        # swallows the rest of the line (and its trailing comment) as literal text.
        if char == "'" and current and current[-1].isdigit() and nxt.isdigit():
            current.append(char)
            i += 1
            continue

        # normal string / char literal
        if char in ('"', "'"):
            quote = char
            current.append(char)
            i += 1
            while i < length:
                if text[i] == "\\":
                    current.extend(text[i:i + 2])
                    i += 2
                    continue
                current.append(text[i])
                if text[i] == quote:
                    i += 1
                    break
                if text[i] == "\n":
                    flush()
                i += 1
            continue

        current.append(char)
        i += 1

    flush()
    return out


def cjk_lines(text: str) -> list[int]:
    hits: list[int] = []
    for number, line in enumerate(text.split("\n"), start=1):
        if any("\u4e00" <= ch <= "\u9fff" for ch in line):
            hits.append(number)
    return hits


def main() -> int:
    parser = argparse.ArgumentParser(description="Check that only comments changed.")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    failures: list[str] = []
    pending: list[tuple[str, list[int]]] = []
    checked = 0

    for top in DIRS:
        for path in sorted((ROOT / top).rglob("*")):
            if path.suffix not in (".hpp", ".cpp"):
                continue
            relative = path.relative_to(ROOT).as_posix()
            checked += 1

            raw = path.read_bytes()
            if raw.startswith(b"\xef\xbb\xbf"):
                failures.append(f"{relative}: file has a UTF-8 BOM")
            if b"\r\n" in raw:
                failures.append(f"{relative}: file has CRLF line endings")

            working = raw.decode("utf-8", errors="replace")
            head = git_show(relative)
            if head is None:
                failures.append(f"{relative}: not present in HEAD (new file?)")
                continue

            if cjk_lines(working):
                pending.append((relative, cjk_lines(working)))

            before = strip_comments(head)
            after = strip_comments(working)
            if before == after:
                if args.verbose:
                    print(f"OK      {relative}")
                continue

            limit = max(len(before), len(after))
            for index in range(limit):
                old = before[index] if index < len(before) else "<missing>"
                new = after[index] if index < len(after) else "<missing>"
                if old != new:
                    failures.append(f"{relative}: code line {index + 1} changed\n"
                                    f"    HEAD : {old}\n"
                                    f"    NOW  : {new}")
                    break

    print(f"checked {checked} file(s)")
    if pending:
        total = sum(len(lines) for _, lines in pending)
        print(f"\nstill containing Chinese: {len(pending)} file(s), {total} line(s)")
        for relative, lines in pending:
            preview = ", ".join(str(n) for n in lines[:8])
            more = "" if len(lines) <= 8 else f", ... (+{len(lines) - 8})"
            print(f"  {relative}: {len(lines)} line(s) -> {preview}{more}")
    if failures:
        print(f"\nFAIL {len(failures)}:")
        for item in failures:
            print(f"  {item}")
        return 1
    print("\nOK: comments-only change confirmed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

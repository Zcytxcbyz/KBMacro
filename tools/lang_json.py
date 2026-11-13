#!/usr/bin/env python3
"""Keep res/langs/*.rc and res/langs/*.json in sync.

At runtime the JSON tables win; the .rc string tables survive as the fallback
for en-US / zh-CN. Both sides therefore describe the same set of string ids.

    export   .rc  -> .json   bootstrap, or re-sync after editing the .rc
    import   .json -> .rc    surgical: only the quoted text is rewritten
    check    report drift, write nothing (exit code 1 when out of sync)

"import" preserves the .rc layout: comments, blank lines, section headers and
the alignment column survive, because only the text between the quotes is
replaced. Ids that exist in the JSON but not in the .rc are appended after the
last entry, aligned with the widest id of that file.

Only lines whose text really changes are touched, so the diff stays small.

Usage:
    python tools/lang_json.py export
    python tools/lang_json.py import
    python tools/lang_json.py check
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RES = ROOT / "res"

# Only these two have a fallback string table; every other language is JSON only.
LANGS = ("en-US", "zh-CN")

MIN_GAP = 2

DEFINE = re.compile(r"^\s*#define\s+(IDS_[A-Z0-9_]+)\s+(\d+)\s*$")
ENTRY = re.compile(r'^(\s*L\((IDS_[A-Z0-9_]+)\)\s+)"((?:[^"\\]|\\.)*)"(.*)$')
PREFIX = re.compile(r"^\s*L\(IDS_[A-Z0-9_]+\)\s+\"")

RC_ESCAPE = {"\\": "\\\\", '"': '\\"', "\n": "\\n", "\t": "\\t", "\r": "\\r"}
RC_UNESCAPE = {"\\": "\\", '"': '"', "n": "\n", "t": "\t", "r": "\r"}


def read_text(path: Path) -> tuple[str, bool]:
    raw = path.read_bytes()
    has_bom = raw.startswith(b"\xef\xbb\xbf")
    return raw.decode("utf-8-sig"), has_bom


def write_text(path: Path, text: str, has_bom: bool) -> None:
    data = text.encode("utf-8")
    if has_bom:
        data = b"\xef\xbb\xbf" + data
    path.write_bytes(data)


def rc_path(code: str) -> Path:
    return RES / "langs" / f"{code}.rc"


def json_path(code: str) -> Path:
    return RES / "langs" / f"{code}.json"


def load_ids() -> dict[str, int]:
    text, _ = read_text(RES / "resource.h")
    ids: dict[str, int] = {}
    for line in text.splitlines():
        match = DEFINE.match(line)
        if match:
            ids[match.group(1)] = int(match.group(2))
    if not ids:
        raise SystemExit("no IDS_* defines found in res/resource.h")
    return ids


def unescape(text: str) -> str:
    out: list[str] = []
    index = 0
    while index < len(text):
        char = text[index]
        if char == "\\" and index + 1 < len(text):
            nxt = text[index + 1]
            if nxt in RC_UNESCAPE:
                out.append(RC_UNESCAPE[nxt])
                index += 2
                continue
        out.append(char)
        index += 1
    return "".join(out)


def escape(text: str) -> str:
    return "".join(RC_ESCAPE.get(char, char) for char in text)


def parse_rc(code: str, ids: dict[str, int]) -> tuple[list[str], bool, dict[str, str]]:
    """Return (lines, has_bom, id string -> text)."""
    text, has_bom = read_text(rc_path(code))
    lines = text.splitlines()
    table: dict[str, str] = {}
    for line in lines:
        match = ENTRY.match(line)
        if not match:
            continue
        name = match.group(2)
        if name not in ids:
            raise SystemExit(f"{rc_path(code).name}: {name} is not defined in res/resource.h")
        table[str(ids[name])] = unescape(match.group(3))
    return lines, has_bom, table


def read_json(code: str) -> dict[str, str]:
    path = json_path(code)
    if not path.exists():
        raise SystemExit(f"{path.relative_to(ROOT)} is missing, run: python tools/lang_json.py export")
    data = json.loads(path.read_text(encoding="utf-8"))
    return {str(key): str(value) for key, value in data.items()}


def write_json(code: str, table: dict[str, str]) -> None:
    ordered = {key: table[key] for key in sorted(table, key=int)}
    json_path(code).write_text(
        json.dumps(ordered, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
        )


def do_export() -> int:
    ids = load_ids()
    for code in LANGS:
        _, _, table = parse_rc(code, ids)
        write_json(code, table)
        print(f"{json_path(code).relative_to(ROOT)}: {len(table)} entr(ies) from .rc")
    return 0


def do_import() -> int:
    ids = load_ids()
    name_of = {str(number): name for name, number in ids.items()}

    for code in LANGS:
        table = read_json(code)
        unknown = [key for key in table if key not in name_of]
        if unknown:
            raise SystemExit(f"{json_path(code).name}: id {unknown[0]} is not defined in res/resource.h")

        lines, has_bom, _ = parse_rc(code, ids)

        # Alignment column used by the existing entries of this file.
        widths = [len(match.group(1).rstrip()) for line in lines if (match := ENTRY.match(line))]
        column = (max(widths) if widths else 0) + MIN_GAP

        replaced = 0
        seen: set[str] = set()
        out: list[str] = []
        last_entry = -1

        for line in lines:
            match = ENTRY.match(line)
            if not match:
                out.append(line)
                continue

            key = str(ids[match.group(2)])
            seen.add(key)
            last_entry = len(out)

            if key not in table:
                out.append(line)      # keep the .rc entry, report it below
                continue

            wanted = escape(table[key])
            if match.group(3) == wanted:
                out.append(line)
                continue

            out.append(match.group(1) + '"' + wanted + '"' + match.group(4))
            replaced += 1

        missing = sorted(set(table) - seen, key=int)
        appended = 0
        if missing:
            fresh = [
                f"    L({name_of[key]})".ljust(column) + '"' + escape(table[key]) + '"'
                for key in missing
            ]
            position = last_entry + 1 if last_entry >= 0 else len(out)
            out[position:position] = fresh
            appended = len(fresh)

        write_text(rc_path(code), "\n".join(out) + "\n", has_bom)
        print(
            f"{rc_path(code).relative_to(ROOT)}: {replaced} rewritten, "
            f"{appended} appended, {len(table) - replaced - appended} unchanged"
        )

    return 0


def do_check() -> int:
    ids = load_ids()
    drift = 0

    for code in LANGS:
        _, _, rc_table = parse_rc(code, ids)
        json_table = read_json(code)

        only_rc = sorted(set(rc_table) - set(json_table), key=int)
        only_json = sorted(set(json_table) - set(rc_table), key=int)
        changed = sorted(
            (key for key in set(rc_table) & set(json_table) if rc_table[key] != json_table[key]),
            key=int,
        )

        if not (only_rc or only_json or changed):
            print(f"{code}: in sync ({len(rc_table)} entr(ies))")
            continue

        drift += 1
        print(
            f"{code}: {len(only_rc)} only in .rc, {len(only_json)} only in .json, "
            f"{len(changed)} different"
        )
        for key in only_rc[:5]:
            print(f"  - {key}  only in .rc      ({rc_table[key]!r})")
        for key in only_json[:5]:
            print(f"  + {key}  only in .json    ({json_table[key]!r})")
        for key in changed[:5]:
            print(f"  ~ {key}  .rc={rc_table[key]!r}  .json={json_table[key]!r}")

    return 1 if drift else 0


def main(argv: list[str]) -> int:
    if len(argv) != 2 or argv[1] not in ("export", "import", "check"):
        print(__doc__.strip())
        return 2

    if argv[1] == "export":
        return do_export()
    if argv[1] == "import":
        return do_import()
    return do_check()


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

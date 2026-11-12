#!/usr/bin/env python3
"""One-off: translate the remaining Chinese comments in res/resource.h."""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CHANGES = {
    2: "// Microsoft Visual C++ generated include file.",
    3: "// Used by resource.rc",
    13: "// ---- RC data resources ----",
    22: "// ---- Common (shared across panels) ----",
    41: "// ---- Parameter / script import-export (shared across panels) ----",
    58: "// ---- Coordinate adaptation dialog ----",
    64: "// ---- Clicker panel ----",
    86: "// ---- Keyboard panel ----",
    96: "// ---- Recorder panel ----",
    118: "// ---- Recorder delivery ----",
    127: "// ---- Settings panel ----",
    175: "// ---- Settings: recording ----",
    184: "// ---- About panel ----",
    196: "// ---- Hotkey capture dialog ----",
    202: "// ---- Coordinate capture overlay ----",
    205: "// ---- Status bar ----",
    216: "// ---- Action edit dialog ----",
}


def main() -> int:
    path = ROOT / "res" / "resource.h"
    with path.open("r", encoding="utf-8", newline="") as handle:
        raw = handle.read()

    crlf = "\r\n" in raw
    lines = raw.splitlines()

    for number, new_text in CHANGES.items():
        if not 1 <= number <= len(lines):
            raise SystemExit(f"line {number} is out of range")
        lines[number - 1] = new_text

    data = ("\r\n" if crlf else "\n").join(lines)
    if raw.endswith(("\n", "\r\n")):
        data += "\r\n" if crlf else "\n"

    with path.open("w", encoding="utf-8", newline="") as handle:
        handle.write(data)

    print(f"res/resource.h: {len(CHANGES)} line(s) translated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Fetch third-party license texts for KBMacro.

Downloads the pinned upstream license of every dependency into
res/licenses/<Name>.txt (they are compiled into the executable as RCDATA and
shown on the About page) and regenerates THIRD_PARTY_LICENSES.txt in the repo
root.

Usage:
    python tools/fetch_licenses.py            # fetch and update
    python tools/fetch_licenses.py --check    # verify only, exit 1 when broken

The script never writes a file whose content looks wrong (placeholder, too
short), so a broken download cannot silently ship.
"""

from __future__ import annotations

import argparse
import sys
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LICENSE_DIR = ROOT / "res" / "licenses"
COMBINED = ROOT / "THIRD_PARTY_LICENSES.txt"

MIN_BYTES = 200
PLACEHOLDER = "[Placeholder]"
SEPARATOR = "=" * 80


@dataclass(frozen=True)
class Library:
    name: str
    version: str
    license_name: str
    out_file: str
    urls: tuple[str, ...] = ()
    local_source: str | None = None


# Keep in sync with conanfile.txt and res/resource.rc (RCDATA block).
LIBRARIES: list[Library] = [
    Library("KBMacro", "0.0.1", "GPL-3.0", "KBMacro.txt", local_source="LICENSE.txt"),
    Library(
        "wxWidgets",
        "3.2.11",
        "wxWindows Library Licence",
        "wxWidgets.txt",
        urls=("https://raw.githubusercontent.com/wxWidgets/wxWidgets/v3.2.11/docs/licence.txt",),
    ),
    Library(
        "spdlog",
        "1.17.0",
        "MIT",
        "spdlog.txt",
        urls=("https://raw.githubusercontent.com/gabime/spdlog/v1.17.0/LICENSE",),
    ),
    Library(
        "nlohmann-json",
        "3.12.0",
        "MIT",
        "nlohmann-json.txt",
        urls=("https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT",),
    ),
    Library(
        "WIL",
        "1.0.260126.7",
        "MIT",
        "WIL.txt",
        urls=(
            "https://raw.githubusercontent.com/microsoft/wil/1.0.260126.7/LICENSE",
            "https://raw.githubusercontent.com/microsoft/wil/master/LICENSE",
        ),
    ),
    Library(
        "moodycamel::ConcurrentQueue",
        "1.0.5",
        "BSD 2-Clause",
        "ConcurrentQueue.txt",
        urls=("https://raw.githubusercontent.com/cameron314/concurrentqueue/v1.0.5/LICENSE.md",),
    ),
    Library(
        "GoogleTest",
        "1.18.0",
        "BSD 3-Clause",
        "GoogleTest.txt",
        urls=(
            "https://raw.githubusercontent.com/google/googletest/v1.18.0/LICENSE",
            "https://raw.githubusercontent.com/google/googletest/main/LICENSE",
        ),
    ),
]


def normalize(raw: bytes) -> str:
    """Decode and normalize to plain UTF-8 with LF endings."""
    text = raw.decode("utf-8", errors="replace")
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    lines = [line.rstrip() for line in text.split("\n")]
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines) + "\n"


def problems_of(text: str) -> list[str]:
    issues: list[str] = []
    if PLACEHOLDER in text:
        issues.append("still contains the placeholder")
    if len(text) < MIN_BYTES:
        issues.append(f"too short ({len(text)} bytes, need >= {MIN_BYTES})")
    return issues


def download(url: str) -> str:
    request = urllib.request.Request(
        url, headers={"User-Agent": "KBMacro-license-fetch/1.0"}
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        if response.status != 200:
            raise urllib.error.HTTPError(url, response.status, "unexpected status", None, None)
        return normalize(response.read())


def acquire(lib: Library) -> tuple[str | None, str]:
    """Return (text, origin). Text is None when every source failed."""
    if lib.local_source is not None:
        path = ROOT / lib.local_source
        if not path.is_file():
            return None, f"missing local file {lib.local_source}"
        return normalize(path.read_bytes()), lib.local_source

    errors: list[str] = []
    for url in lib.urls:
        try:
            return download(url), url
        except (urllib.error.URLError, OSError, TimeoutError) as exc:
            errors.append(f"{url} -> {exc}")
    return None, "; ".join(errors)


def write_if_changed(path: Path, text: str) -> bool:
    if path.is_file() and path.read_text(encoding="utf-8", errors="replace") == text:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    # newline="" keeps the LF endings we produced on Windows too
    with path.open("w", encoding="utf-8", newline="") as handle:
        handle.write(text)
    return True


def build_combined() -> str:
    parts = [
        "KBMacro - third-party licenses",
        "",
        "KBMacro itself is licensed under the GNU General Public License v3.0",
        "(see LICENSE.txt). The components below are bundled under their own terms.",
        "",
    ]
    for lib in LIBRARIES:
        if lib.local_source is not None:
            continue
        path = LICENSE_DIR / lib.out_file
        if not path.is_file():
            continue
        parts += [
            SEPARATOR,
            f"{lib.name} {lib.version}  |  {lib.license_name}",
            SEPARATOR,
            "",
            normalize(path.read_bytes()).rstrip("\n"),
            "",
            "",
        ]
    return "\n".join(parts).rstrip("\n") + "\n"


def run_check() -> int:
    failed = False
    for lib in LIBRARIES:
        path = LICENSE_DIR / lib.out_file
        if not path.is_file():
            print(f"MISSING  {path.relative_to(ROOT)}")
            failed = True
            continue
        issues = problems_of(normalize(path.read_bytes()))
        if issues:
            print(f"BAD      {path.relative_to(ROOT)}: {'; '.join(issues)}")
            failed = True
        else:
            print(f"OK       {path.relative_to(ROOT)} ({path.stat().st_size} bytes)")
    if not COMBINED.is_file():
        print(f"MISSING  {COMBINED.name}")
        failed = True
    return 1 if failed else 0


def run_update() -> int:
    failed = False
    for lib in LIBRARIES:
        path = LICENSE_DIR / lib.out_file
        text, origin = acquire(lib)
        if text is None:
            print(f"FAILED   {lib.name}: {origin}")
            failed = True
            continue
        issues = problems_of(text)
        if issues:
            print(f"FAILED   {lib.name}: {origin} -> {'; '.join(issues)}")
            failed = True
            continue
        changed = write_if_changed(path, text)
        print(f"{'UPDATED ' if changed else 'unchanged'} {path.relative_to(ROOT)}"
              f" ({len(text)} bytes, {origin})")

    if write_if_changed(COMBINED, build_combined()):
        print(f"UPDATED  {COMBINED.name}")
    else:
        print(f"unchanged {COMBINED.name}")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Fetch third-party license texts.")
    parser.add_argument("--check", action="store_true",
                        help="only verify res/licenses, never download")
    args = parser.parse_args()
    return run_check() if args.check else run_update()


if __name__ == "__main__":
    sys.exit(main())

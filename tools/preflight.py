#!/usr/bin/env python3
"""Run the three checks that must pass before a commit.

    1. language tables   python tools/lang_json.py check  (en-US / zh-CN vs .rc)
    2. formatting        clang-format --dry-run --Werror over src/ and tests/
    3. tests             ctest --test-dir build/<preset>

clang-format is discovered, never hardcoded: --clang-format, then the
CLANG_FORMAT environment variable, then PATH, then the Visual Studio install
located through vswhere.exe. The repository therefore holds no machine-specific
paths.

Usage:
    python tools/preflight.py
    python tools/preflight.py --preset x64-debug-user
    python tools/preflight.py --skip-tests
    python tools/preflight.py --fix          # reformat the drifted files in place
"""

from __future__ import annotations

import argparse
import glob
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCES = ("src", "tests")
SUFFIXES = (".hpp", ".cpp")


def say(tag: str, message: str) -> None:
    print(f"[{tag}] {message}")


def find_clang_format(explicit: str | None) -> Path | None:
    if explicit:
        path = Path(explicit)
        return path if path.exists() else None

    from_env = os.environ.get("CLANG_FORMAT")
    if from_env:
        path = Path(from_env)
        if path.exists():
            return path

    on_path = shutil.which("clang-format")
    if on_path:
        return Path(on_path)

    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None

    proc = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
        capture_output=True,
        text=True,
        check=False,
    )
    roots = [line.strip() for line in proc.stdout.splitlines() if line.strip()]
    if not roots:
        return None

    pattern = str(Path(roots[0]) / "VC" / "Tools" / "Llvm" / "**" / "clang-format.exe")
    matches = sorted(glob.glob(pattern, recursive=True))
    return Path(matches[0]) if matches else None


def find_ctest() -> Path | None:
    on_path = shutil.which("ctest")
    if on_path:
        return Path(on_path)

    cmake = shutil.which("cmake")
    if cmake:
        sibling = Path(cmake).with_name("ctest.exe")
        if sibling.exists():
            return sibling
    return None


def source_files() -> list[Path]:
    files: list[Path] = []
    for folder in SOURCES:
        for path in sorted((ROOT / folder).rglob("*")):
            if path.suffix in SUFFIXES:
                files.append(path)
    return files


def check_language_tables() -> bool:
    script = ROOT / "tools" / "lang_json.py"
    if not script.exists():
        say("skip", "tools/lang_json.py not found")
        return True

    proc = subprocess.run([sys.executable, str(script), "check"], cwd=ROOT, check=False)
    return proc.returncode == 0


def check_format(clang_format: Path | None, fix: bool) -> bool:
    if clang_format is None:
        say("skip", "clang-format not found (use --clang-format or set CLANG_FORMAT)")
        return True

    files = source_files()
    drifted: list[Path] = []
    for path in files:
        proc = subprocess.run(
            [str(clang_format), "--style=file", "-i" if fix else "--dry-run", *([] if fix else ["--Werror"]), str(path)],
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if not fix and proc.returncode != 0:
            drifted.append(path)

    if fix:
        say("fix", f"reformatted {len(files)} file(s) with {clang_format}")
        return True

    if not drifted:
        say("ok", f"formatting ({len(files)} file(s))")
        return True

    say("fail", f"formatting: {len(drifted)} file(s) need clang-format")
    for path in drifted:
        print(f"       {path.relative_to(ROOT)}")
    print("       run with --fix, or reformat these files before committing")
    return False


def check_tests(preset: str) -> bool:
    ctest = find_ctest()
    if ctest is None:
        say("skip", "ctest not found (add cmake's bin directory to PATH)")
        return True

    build_dir = ROOT / "build" / preset.removeprefix("x64-")
    if not build_dir.exists():
        say("skip", f"{build_dir.relative_to(ROOT)} does not exist, configure the preset first")
        return True

    proc = subprocess.run(
        [str(ctest), "--test-dir", str(build_dir), "--output-on-failure"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if proc.returncode == 0:
        say("ok", f"ctest ({build_dir.relative_to(ROOT)})")
        return True

    say("fail", f"ctest ({build_dir.relative_to(ROOT)})")
    print(proc.stdout.strip())
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Run the pre-commit checks.")
    parser.add_argument("--preset", default="x64-debug-user", help="preset whose build directory holds the tests")
    parser.add_argument("--clang-format", dest="clang_format", default=None, help="path to clang-format")
    parser.add_argument("--fix", action="store_true", help="reformat the drifted files instead of reporting them")
    parser.add_argument("--skip-lang", action="store_true", help="skip the language-table check")
    parser.add_argument("--skip-format", action="store_true", help="skip the formatting check")
    parser.add_argument("--skip-tests", action="store_true", help="skip ctest")
    args = parser.parse_args()

    results: list[tuple[str, bool]] = []

    if not args.skip_lang:
        say("run", "language tables")
        results.append(("language tables", check_language_tables()))

    if not args.skip_format:
        say("run", "formatting")
        results.append(("formatting", check_format(find_clang_format(args.clang_format), args.fix)))

    if not args.skip_tests:
        say("run", f"ctest ({args.preset})")
        results.append(("ctest", check_tests(args.preset)))

    print()
    failed = [name for name, ok in results if not ok]
    for name, ok in results:
        print(f"  {'PASS' if ok else 'FAIL'}  {name}")

    if failed:
        say("fail", f"{len(failed)} check(s) failed: {', '.join(failed)}")
        return 1

    say("ok", "all checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

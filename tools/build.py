#!/usr/bin/env python3
"""Build KBMacro and keep the three version numbers in sync.

The version lives in three places that must agree:
    * git tag            (v1.2.3)          -- release identity
    * CMakeLists.txt     project(... VERSION 1.2.3 ...)
    * res/resource.rc    FILEVERSION / PRODUCTVERSION + FileVersion / ProductVersion

Usage:
    python tools/build.py                       # build with the current version
    python tools/build.py --version 1.2.3       # sync all three, then build
    python tools/build.py --check               # verify only, exit 1 on mismatch
    python tools/build.py --preset x64-debug-user
    python tools/build.py --no-build            # sync version files only
    python tools/build.py --tag                 # tag v1.2.3 after a good build

Run it from a Visual Studio Developer PowerShell (cl.exe and ninja must be on
PATH); Conan 2 is expected as well.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CMAKE_LISTS = ROOT / "CMakeLists.txt"
RESOURCE_RC = ROOT / "res" / "resource.rc"

EXE_NAME = "KBMacro.exe"
DEFAULT_PRESET = "x64-release"

# project(KBMacro VERSION 1.2.3 ...)
RE_CMAKE_VERSION = re.compile(r"(project\(\s*KBMacro\s+VERSION\s+)(\d+(?:\.\d+){0,2})")
RE_RC_FIXED = re.compile(r"((?:FILE|PRODUCT)VERSION)(\s+)(\d+(?:\s*,\s*\d+){3})")
RE_RC_STRING = re.compile(r'((?:VALUE\s+"(?:File|Product)Version",\s*)")(\d+(?:\.\d+){3})(")')


class BuildError(RuntimeError):
    pass


# --------------------------------------------------------------------------- #
# text helpers
# --------------------------------------------------------------------------- #

def read_text(path: Path) -> tuple[str, str, str]:
    """Return (text, encoding, newline) without guessing wrong on CJK sources."""
    raw = path.read_bytes()
    newline = "\r\n" if b"\r\n" in raw else "\n"
    for encoding in ("utf-8-sig", "utf-8", "gbk"):
        try:
            return raw.decode(encoding), encoding, newline
        except UnicodeDecodeError:
            continue
    raise BuildError(f"cannot decode {path}")


def write_text(path: Path, text: str, encoding: str, newline: str) -> bool:
    """Write only when the content actually changes."""
    data = text.replace("\r\n", "\n").replace("\n", newline).encode(encoding)
    if path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


# --------------------------------------------------------------------------- #
# version handling
# --------------------------------------------------------------------------- #

def normalize_version(value: str) -> str:
    parts = value.strip().lstrip("vV").split(".")
    if not 1 <= len(parts) <= 3 or any(not part.isdigit() for part in parts):
        raise BuildError(f"invalid version: {value!r} (expected 1, 1.2 or 1.2.3)")
    parts += ["0"] * (3 - len(parts))
    return ".".join(str(int(part)) for part in parts)


def version_from_cmake() -> str | None:
    match = RE_CMAKE_VERSION.search(CMAKE_LISTS.read_text(encoding="utf-8"))
    return normalize_version(match.group(2)) if match else None


def version_from_rc() -> str | None:
    text, _, _ = read_text(RESOURCE_RC)
    match = re.search(r"FILEVERSION\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)", text)
    if not match:
        return None
    major, minor, patch, _ = match.groups()
    return f"{int(major)}.{int(minor)}.{int(patch)}"


def version_from_tag() -> str | None:
    result = subprocess.run(
        ["git", "describe", "--tags", "--exact-match", "HEAD"],
        cwd=ROOT, capture_output=True, text=True,
    )
    if result.returncode != 0:
        return None
    return normalize_version(result.stdout.strip())


def git_is_clean() -> bool:
    result = subprocess.run(
        ["git", "status", "--porcelain"], cwd=ROOT, capture_output=True, text=True
    )
    return result.returncode == 0 and not result.stdout.strip()


def sync_version(version: str, dry_run: bool) -> list[str]:
    """Write `version` into CMakeLists.txt and res/resource.rc."""
    changed: list[str] = []

    text = CMAKE_LISTS.read_text(encoding="utf-8")
    new_text = RE_CMAKE_VERSION.sub(lambda m: f"{m.group(1)}{version}", text, count=1)
    if new_text != text:
        changed.append(f"CMakeLists.txt -> {version}")
        if not dry_run:
            CMAKE_LISTS.write_text(new_text, encoding="utf-8", newline="\n")

    rc_text, encoding, newline = read_text(RESOURCE_RC)
    fixed = f"{version.replace('.', ', ')}, 0"
    string_value = f"{version}.0"
    new_rc = RE_RC_FIXED.sub(lambda m: f"{m.group(1)}{m.group(2)}{fixed}", rc_text)
    new_rc = RE_RC_STRING.sub(lambda m: f"{m.group(1)}{string_value}{m.group(3)}", new_rc)
    if new_rc != rc_text:
        changed.append(f"res/resource.rc -> {fixed} / {string_value}")
        if not dry_run:
            write_text(RESOURCE_RC, new_rc, encoding, newline)

    return changed


def report(check_only: bool) -> int:
    cmake = version_from_cmake()
    rc = version_from_rc()
    tag = version_from_tag()

    rows = [("git tag", tag or "(none)"), ("CMakeLists.txt", cmake or "(missing)"),
            ("res/resource.rc", rc or "(missing)")]
    width = max(len(name) for name, _ in rows)
    for name, value in rows:
        print(f"  {name.ljust(width)}  {value}")

    known = [value for value in (tag, cmake, rc) if value]
    consistent = len(set(known)) <= 1
    print(f"  {'result'.ljust(width)}  {'OK' if consistent else 'MISMATCH'}")
    if check_only:
        return 0 if consistent else 1
    return 0 if consistent else 1


# --------------------------------------------------------------------------- #
# build
# --------------------------------------------------------------------------- #

def preset_dir(preset: str) -> Path:
    return ROOT / "build" / preset[len("x64-"):]


def preset_rel(preset: str) -> str:
    """Relative, forward-slash form: what the command line should see."""
    return "build/" + preset[len("x64-"):]


def conan_dir(preset: str) -> str:
    """The -user presets reuse the Conan output of their base preset."""
    return "build/" + preset[len("x64-"):].replace("-user", "")


def conan_profile(preset: str) -> str:
    """profiles/<type> pins the Ninja generator and keeps Conan away from
    CMakeUserPresets.json, neither of which the default profile does."""
    return "profiles/" + ("debug" if "debug" in preset else "release")

def build_type_of(preset: str) -> str:
    return "Debug" if "debug" in preset else "Release"

def run(command: list[str], cwd: Path = ROOT) -> None:
    print(f">> {' '.join(command)}")
    result = subprocess.run(command, cwd=cwd)
    if result.returncode != 0:
        raise BuildError(f"command failed ({result.returncode}): {' '.join(command)}")


def preflight() -> None:
    missing = [tool for tool in ("cmake", "conan") if shutil.which(tool) is None]
    if missing:
        raise BuildError(f"not on PATH: {', '.join(missing)}")
    # cl.exe / ninja only exist inside a Visual Studio developer environment.
    if shutil.which("cl") is None or shutil.which("ninja") is None:
        raise BuildError(
            "cl.exe / ninja not found -- start this script from a "
            "'x64 Native Tools Command Prompt for VS' or Developer PowerShell"
        )


def exe_file_version(path: Path) -> str | None:
    """Read VS_FIXEDFILEINFO's file version straight from the built exe."""
    if not path.is_file():
        return None
    import ctypes
    from ctypes import wintypes

    version = ctypes.WinDLL("version", use_last_error=True)
    size = version.GetFileVersionInfoSizeW(str(path), None)
    if not size:
        return None
    buffer = ctypes.create_string_buffer(size)
    if not version.GetFileVersionInfoW(str(path), 0, size, buffer):
        return None
    pointer = ctypes.c_void_p()
    length = wintypes.UINT()
    if not version.VerQueryValueW(buffer, "\\", ctypes.byref(pointer), ctypes.byref(length)):
        return None
    fields = ctypes.cast(pointer, ctypes.POINTER(wintypes.DWORD))
    ms, ls = fields[2], fields[3]
    return f"{ms >> 16}.{ms & 0xFFFF}.{ls >> 16}.{ls & 0xFFFF}"


def build(preset: str) -> Path:
    preflight()
    binary_dir = preset_dir(preset)
    build_type = build_type_of(preset)

    profile = conan_profile(preset)
    if not (ROOT / profile).is_file():
        raise BuildError(f"missing conan profile: {profile}")

    run(["conan", "install", ".", f"--output-folder={conan_dir(preset)}",
         "--build=missing", f"--profile={profile}"])
    run(["cmake", "--preset", preset])
    run(["cmake", "--build", preset_rel(preset)])

    exe = binary_dir / "bin" / EXE_NAME
    if not exe.is_file():
        raise BuildError(f"build finished but {exe} is missing")
    print(f"   exe      {exe}")
    print(f"   size     {exe.stat().st_size:,} bytes")
    print(f"   version  {exe_file_version(exe) or '(unknown)'}")
    return exe


def make_tag(version: str) -> None:
    if not git_is_clean():
        raise BuildError("working tree is dirty -- commit before tagging")
    tag = f"v{version}"
    existing = subprocess.run(
        ["git", "tag", "--list", tag], cwd=ROOT, capture_output=True, text=True
    ).stdout.strip()
    if existing:
        print(f"   tag      {tag} already exists, skipped")
        return
    run(["git", "tag", "-a", tag, "-m", f"KBMacro {version}"])
    print(f"   tag      {tag} created")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build KBMacro and sync versions.")
    parser.add_argument("--version", help="target version, e.g. 1.2.3")
    parser.add_argument("--preset", default=DEFAULT_PRESET, help="CMake preset to use")
    parser.add_argument("--no-build", action="store_true", help="only sync version files")
    parser.add_argument("--check", action="store_true", help="verify versions and exit")
    parser.add_argument("--dry-run", action="store_true", help="show changes, write nothing")
    parser.add_argument("--tag", action="store_true", help="create v<version> tag after build")
    args = parser.parse_args()

    try:
        target = normalize_version(args.version) if args.version else None
        if target is None:
            target = version_from_cmake()
            if target is None:
                raise BuildError("CMakeLists.txt has no project(VERSION ...) to start from")

        print("version:")
        if not args.check and not args.dry_run:
            changed = sync_version(target, dry_run=False)
            for line in changed:
                print(f"   updated  {line}")
        elif args.dry_run:
            for line in sync_version(target, dry_run=True):
                print(f"   would update  {line}")

        status = report(args.check or args.dry_run)
        if args.check:
            return status
        if args.no_build or args.dry_run:
            return status

        print("build:")
        build(args.preset)

        if args.tag:
            make_tag(target)
        return status
    except BuildError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Generate src/common/KeyNames.gen.hpp.

Requirements:
    pip install pywin32

Usage:
    python tools/generate_keynames.py

Notes:
    - VK -> display name is queried from the Windows GetKeyNameTextW API.
    - The generated header is committed to the repository, so building the
      project never needs Python.
    - Regenerate only when the set of Windows keys or the naming rules change.
    - Names can differ between Windows versions, which is exactly why the
      output is pinned in version control.
"""

import ctypes
from ctypes import wintypes
from pathlib import Path

import win32con

# ---- Win32 API bindings ----
user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.MapVirtualKeyW.restype = wintypes.UINT
user32.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
user32.GetKeyNameTextW.restype = ctypes.c_int
user32.GetKeyNameTextW.argtypes = [ctypes.c_long, wintypes.LPWSTR, ctypes.c_int]

MAPVK_VK_TO_VSC = 0

# Extended-scancode keys need bit 0x100 set, otherwise GetKeyNameTextW reports
# the numeric-pad variant (PageUp, arrows, Insert, ... would all be wrong).
EXTENDED_VKS = {
    win32con.VK_PRIOR, win32con.VK_NEXT,
    win32con.VK_HOME, win32con.VK_END,
    win32con.VK_INSERT, win32con.VK_DELETE,
    win32con.VK_LEFT, win32con.VK_RIGHT,
    win32con.VK_UP, win32con.VK_DOWN,
    win32con.VK_DIVIDE,
    win32con.VK_RCONTROL, win32con.VK_RMENU,
    win32con.VK_NUMLOCK,
}

# Excluded on purpose: mouse buttons, the modifier keys (they are stored as
# bits in hk::kMod*), IME related keys, and keys Windows reports but nobody can
# press on a normal keyboard.
EXCLUDE_NAMES = [
    # Mouse buttons
    "VK_LBUTTON", "VK_RBUTTON", "VK_MBUTTON",
    "VK_XBUTTON1", "VK_XBUTTON2", "VK_CANCEL",
    # Modifier keys (main and left/right variants)
    "VK_SHIFT", "VK_CONTROL", "VK_MENU", "VK_LWIN", "VK_RWIN",
    "VK_LSHIFT", "VK_RSHIFT",
    "VK_LCONTROL", "VK_RCONTROL",
    "VK_LMENU", "VK_RMENU",
    # IME / input method
    "VK_KANA", "VK_HANGEUL", "VK_HANGUL", "VK_JUNJA", "VK_FINAL",
    "VK_HANJA", "VK_KANJI",
    "VK_CONVERT", "VK_NONCONVERT", "VK_ACCEPT", "VK_MODECHANGE",
    "VK_IME_ON", "VK_IME_OFF", "VK_PROCESSKEY",
    # Legacy / reserved
    "VK_CLEAR", "VK_SELECT", "VK_PRINT", "VK_EXECUTE", "VK_HELP",
    "VK_ATTN", "VK_CRSEL", "VK_EXSEL", "VK_EREOF", "VK_PLAY",
    "VK_ZOOM", "VK_PA1", "VK_OEM_CLEAR", "VK_PACKET",
]

EXCLUDE_EXTRA = {0xC1, 0xC2}

EXCLUDE = {getattr(win32con, n) for n in EXCLUDE_NAMES
           if hasattr(win32con, n)} | EXCLUDE_EXTRA


def vk_to_name(vk: int) -> str | None:
    """Return the display name of a virtual-key code, or None to skip it."""
    if vk in EXCLUDE:
        return None

    sc = user32.MapVirtualKeyW(vk, MAPVK_VK_TO_VSC)
    if sc == 0:
        return None

    if vk in EXTENDED_VKS:
        sc |= 0x100

    buf = ctypes.create_unicode_buffer(64)
    n = user32.GetKeyNameTextW(sc << 16, buf, 64)
    if n <= 0:
        return None
    return buf.value


def generate_entries() -> list[tuple[int, str]]:
    """Walk the VK value space (0x08-0xFF) and keep unique display names."""
    seen: dict[str, int] = {}
    entries: list[tuple[int, str]] = []

    for vk in range(0x08, 0x100):
        name = vk_to_name(vk)
        if not name:
            continue
        if name in seen:
            continue
        seen[name] = vk
        entries.append((vk, name))

    return entries


def escape_cpp_string(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')


def render_header(entries: list[tuple[int, str]]) -> str:
    lines = [
        "// Generated file -- do not edit by hand.",
        "// Regenerate with: python tools/generate_keynames.py",
        "// Requirements: pip install pywin32",
        "//",
        "// Names are queried from the Windows GetKeyNameTextW API and therefore",
        "// match what the user sees on their own layout. The output is committed",
        "// so that builds stay reproducible across Windows versions.",
        "",
        "#pragma once",
        "",
        "struct KeyNameEntry {",
        "    unsigned vk;",
        "    const wchar_t *name;",
        "};",
        "",
        "inline constexpr KeyNameEntry kKeyNames[] = {",
    ]

    for vk, name in entries:
        lines.append(f'    {{0x{vk:02X}, L"{escape_cpp_string(name)}"}},')

    lines += [
        "};",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    entries = generate_entries()
    header = render_header(entries)

    out_path = (Path(__file__).resolve().parent.parent
                / "src" / "common" / "KeyNames.gen.hpp")
    out_path.write_text(header, encoding="utf-8", newline="\n")

    print(f"wrote {out_path}")
    print(f"  entries: {len(entries)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

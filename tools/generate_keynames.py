#!/usr/bin/env python3
"""
生成 src/common/KeyNames.gen.hpp

依赖：
    pip install pywin32

运行：
    python tools/generate_keynames.py

设计说明：
    - VK → 显示名 通过 Windows 的 GetKeyNameTextW API 获取
    - 生成结果在生成机器的语言与键盘布局下固定，发布后所有用户看到一致
      这是有意为之：键名稳定可控，不随用户系统飘
    - 生成的头文件提交到仓库，构建不依赖 Python
    - 只在"Windows 加了新 VK"或"想换生成环境"时重新运行
"""

import ctypes
from ctypes import wintypes
from pathlib import Path

import win32con

# ---- Win32 API 绑定 ----
user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.MapVirtualKeyW.restype = wintypes.UINT
user32.MapVirtualKeyW.argtypes = [wintypes.UINT, wintypes.UINT]
user32.GetKeyNameTextW.restype = ctypes.c_int
user32.GetKeyNameTextW.argtypes = [ctypes.c_long, wintypes.LPWSTR, ctypes.c_int]

MAPVK_VK_TO_VSC = 0

# 扩展键：scancode 需要 | 0x100（否则 PageUp / 方向键等会返回错名）
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

# 排除项：鼠标、修饰键（走位掩码）、IME、保留 / 无意义键
EXCLUDE_NAMES = [
    # 鼠标
    "VK_LBUTTON", "VK_RBUTTON", "VK_MBUTTON",
    "VK_XBUTTON1", "VK_XBUTTON2", "VK_CANCEL",
    # 修饰键（抽象）
    "VK_SHIFT", "VK_CONTROL", "VK_MENU", "VK_LWIN", "VK_RWIN",
    # 左右修饰键（走 hk::kMod* 位掩码）
    "VK_LSHIFT", "VK_RSHIFT",
    "VK_LCONTROL", "VK_RCONTROL",
    "VK_LMENU", "VK_RMENU",
    # IME / 输入法
    "VK_KANA", "VK_HANGEUL", "VK_HANGUL", "VK_JUNJA", "VK_FINAL",
    "VK_HANJA", "VK_KANJI",
    "VK_CONVERT", "VK_NONCONVERT", "VK_ACCEPT", "VK_MODECHANGE",
    "VK_IME_ON", "VK_IME_OFF", "VK_PROCESSKEY",
    # 保留 / 无意义
    "VK_CLEAR", "VK_SELECT", "VK_PRINT", "VK_EXECUTE", "VK_HELP",
    "VK_ATTN", "VK_CRSEL", "VK_EXSEL", "VK_EREOF", "VK_PLAY",
    "VK_ZOOM", "VK_PA1", "VK_OEM_CLEAR", "VK_PACKET",
]

EXCLUDE_EXTRA = {0xC1, 0xC2}

EXCLUDE = {getattr(win32con, n) for n in EXCLUDE_NAMES
           if hasattr(win32con, n)} | EXCLUDE_EXTRA


def vk_to_name(vk: int) -> str | None:
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
    """遍历 VK 编码空间（0x08–0xFF），取有名字的键，按名去重。"""
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
        "// 自动生成，请勿手改。",
        "// 重新生成：python tools/generate_keynames.py",
        "// 依赖：pip install pywin32",
        "//",
        "// 键名在生成机器的 Windows 语言与键盘布局下固定，",
        "// 发布后所有用户看到的显示名一致，不随系统变化。",
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


def main():
    entries = generate_entries()
    header = render_header(entries)

    out_path = (Path(__file__).resolve().parent.parent
                / "src" / "common" / "KeyNames.gen.hpp")
    out_path.write_text(header, encoding="utf-8", newline="\n")

    print(f"wrote {out_path}")
    print(f"  entries: {len(entries)}")


if __name__ == "__main__":
    main()

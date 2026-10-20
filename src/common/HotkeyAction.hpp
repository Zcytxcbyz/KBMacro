#pragma once

#include <iterator>

// ReSharper disable once CppUnusedIncludeDirective
#include "resource.h"

// ============================================================
// 全局热键动作
//
// 枚举值即热键数组索引，与 Config::hotkeys 一一对应。
// 顺序变化会破坏已保存的配置文件，不可随意调整。
// ============================================================

enum class HotkeyAction {
    StartClicker,
    StopClicker,
    StartKeyboard,
    StopKeyboard,
    SendOnce,
    StartRecord,
    StopRecord,
    StartPlayback,
    StopPlayback,
    StartAll,
    StopAll,
};

// 热键数量。Config / HotkeyManager / SettingsPanel 共用。
inline constexpr int kHotkeyCount = 11;

// 动作 → 显示名资源 ID。SettingsPanel 直接按索引取。
inline constexpr int kHotkeyActionResIds[kHotkeyCount] = {
    IDS_SETTINGS_HOTKEY_START_CLICKER,
    IDS_SETTINGS_HOTKEY_STOP_CLICKER,
    IDS_SETTINGS_HOTKEY_START_KEYBOARD,
    IDS_SETTINGS_HOTKEY_STOP_KEYBOARD,
    IDS_SETTINGS_HOTKEY_SEND_ONCE,
    IDS_SETTINGS_HOTKEY_START_RECORD,
    IDS_SETTINGS_HOTKEY_STOP_RECORD,
    IDS_SETTINGS_HOTKEY_START_PLAYBACK,
    IDS_SETTINGS_HOTKEY_STOP_PLAYBACK,
    IDS_SETTINGS_HOTKEY_START_ALL,
    IDS_SETTINGS_HOTKEY_STOP_ALL,
};

static_assert(std::size(kHotkeyActionResIds) == kHotkeyCount,
              "The hotkey action list doesn't match kHotkeyCount");
